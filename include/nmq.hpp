/*
    Copyright (C) 2010 Erik Rigtorp <erik@rigtorp.com>. 
    All rights reserved.

    This file is part of NanoMQ.

    NanoMQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    NanoMQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NanoMQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NMQ_HPP_
#define NMQ_HPP_

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include <cassert>
#include <cstring>
#include <string>

#include <bones/barrier.h>
#include <bones/cpu.h>
#include <bones/compiler.h>

namespace nmq {

class context_t {

private:

  // POD for header data
  struct header {
    unsigned int nodes;
    unsigned int rings;
    unsigned int size;
    unsigned int msg_size;
  };
  
  typedef volatile unsigned int vo_uint; 
  
  // POD for ring
  struct ring {
    
    unsigned int _size;
    unsigned int _msg_size;
    unsigned int _offset;
    
    // R/W access by the reader
    // R/O access by the writer
    vo_uint       _head;
    
    // R/W access by the writer
    // R/O access by the reader
    vo_uint       _tail;
  };

public:
  
  context_t(const char *fname) {
    fname_.assign(fname);
  }

  bool create(unsigned int nodes, unsigned int size, unsigned int msg_size) {
    int fd = ::open(fname_.c_str(), O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
    if (fd == -1) 
      return false;

    unsigned int real_size = po2(size);
    unsigned int n_rings = 2*(nodes * (nodes - 1)) / 2;
    unsigned int file_size = sizeof(header) + sizeof(ring)*n_rings + n_rings*real_size*msg_size;
    
    if (ftruncate(fd, file_size) == -1) 
      return false;

    p_ = mmap(NULL, file_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p_ == NULL) 
      return false;

    memset(p_, 0, file_size);
    
    header_ = (header*)p_;
    ring_ = (ring*)(header_ + 1);
    data_ = (char*)(ring_ + n_rings);

    header_->nodes = nodes;
    header_->rings = n_rings;
    header_->size = real_size - 1;
    header_->msg_size = msg_size;

    for (unsigned int i = 0; i < header_->rings; i++) {
      ring_[i]._size = real_size - 1;
      ring_[i]._msg_size = header_->msg_size;
      ring_[i]._offset = &data_[i*real_size*msg_size] - data_;
    }
    
    return true;
  }
  
  bool open(unsigned int nodes, unsigned int size, unsigned int msg_size) {
    int fd = ::open(fname_.c_str(), O_RDWR);
    if (fd == -1)
      return create(nodes, size, msg_size);
    
    struct stat buf;
    if (fstat(fd, &buf) == -1) 
      return false;
    unsigned int file_size = buf.st_size;
    
    if (ftruncate(fd, file_size) == -1)
      return false;

    p_ = mmap(NULL, file_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p_ == NULL)
      return false;
    
    header_ = (header*)p_;
    ring_ = (ring*)(header_ + 1);
    data_ = (char*)(ring_ + header_->rings);    
    
    return true;
  }
  
  void print() {
    printf("nodes: %u, size: %u, msgsz: %u\n", header_->nodes, header_->size, header_->msg_size);
    for (unsigned int i = 0; i < header_->rings; i++) {
      printf("%3i: %10u %10u\n", i, ring_[i]._head, ring_[i]._tail);
    }
  }

private:

  // Round up to the power of two	
  static unsigned int po2(unsigned int size) {
    unsigned int i;
    for (i=0; (1U << i) < size; i++);
    return 1U << i;
  }

  ring* get_ring(unsigned int from, unsigned int to) {
    // TODO set errno and return error condition
    assert(p_ != NULL);
    assert(from != to);
    assert(from < header_->nodes);
    assert(to < header_->nodes);
    return &ring_[from*(to - 1) + 1];
  }

  bool send(ring *ring, const void *msg, size_t size) {
    assert(size <= ring->_msg_size);

    unsigned int h = (ring->_head - 1) & ring->_size;
    unsigned int t = ring->_tail;
    if (t == h)
      return false;

    char *d = &data_[ring_->_offset + t*ring->_msg_size];
    memcpy(d, msg, size);
    
    // Barrier is needed to make sure that item is updated 
    // before it's made available to the reader
    bones::barrier::memw();

    ring->_tail = (t + 1) & ring->_size;
    return true;
  }

  bool send(unsigned int from, unsigned int to, const void *msg, size_t size) {
    ring *ring = get_ring(from, to);
    while (!send(ring, msg, size)) bones::cpu::__relax();
    return true;
  }

  bool sendnb(unsigned int from, unsigned int to, const void *msg, size_t size) {
    ring *ring = get_ring(from, to);
    return send(ring, msg, size);
  }

  bool recv(ring *ring, void *msg, size_t size) {
    unsigned int t = ring->_tail;
    unsigned int h = ring->_head;
    if (h == t)
      return false;

    void *d = &data_[ring_->_offset + h*ring->_msg_size];
    memcpy(msg, d, size);

    // Barrier is needed to make sure that we finished reading the item
    // before moving the head
    bones::barrier::comp();

    ring->_head = (h + 1) & ring_->_size;
    return true;
  }

  bool recv(unsigned int from, unsigned int to, void *msg, size_t size) {
    ring *ring = get_ring(from, to);
    while (!recv(ring, msg, size)) bones::cpu::__relax();
    return true;
  }

  bool recvnb(unsigned int from, unsigned int to, void *s, int size) {
    return recv(get_ring(from, to), s, size);
  }
  
  bool recv(unsigned int to, void *msg, size_t size) {
    // TODO "fair" receiving
    while (true) {
      for (unsigned int i = 0; i < header_->nodes; i++) {
        if (to != i && recvnb(i, to, msg, size) != -1) return true;
      }
      bones::cpu::__relax();
    }
    return false;
  }

  bool recvnb(unsigned int to, void *msg, size_t size) {
    // TODO "fair" receiving
    for (unsigned int i = 0; i < header_->nodes; i++) {
      if (to != i && recvnb(i, to, msg, size) != -1) return true;
    }
    return false;
  }



  std::string fname_;
  void *p_;
  header *header_;
  ring *ring_;
  char *data_;

  friend class node_t;

};

class node_t {

  context_t *context_;
  unsigned int node_;

public:

  node_t(context_t &context, unsigned int node) 
    : context_(&context), node_(node) {
    assert(context_ != NULL);
    //assert(node < context_->nodes());
  }

  bool send(unsigned int to, const void *msg, int size) {
    return context_->send(node_, to, msg, size);
  }

  bool sendnb(unsigned int to, const void *msg, int size) {
    return context_->sendnb(node_, to, msg, size);
  }

  bool recv(unsigned int from, void *msg, int size) {
    return context_->recv(from, node_, msg, size);
  }

  bool recvnb(unsigned int from, void *msg, int size) {
    return context_->recvnb(from, node_, msg, size);
  }

  bool recv(void *msg, int size) {
    return context_->recv(node_, msg, size);
  }

  bool recvnb(void *msg, int size) {
    return context_->recvnb(node_, msg, size);
  }

};

}

#endif
