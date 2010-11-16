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

#include <sys/time.h>
#include <errno.h>
#include <cstdlib>
#include <nmq.hpp>

int main(int argc, char* argv[]) {

  if (argc != 4) {
    printf ("usage: remote_thr <queue> <message-size> "
            "<roundtrip-count>\n");
    return 1;
  }
  const char* queue = argv[1];
  size_t message_size = atoi(argv [2]);
  long roundtrip_count = atoi(argv [3]);

  nmq::context_t context(queue);

  if (!context.open(2, 15, message_size)) {
    printf("error in context open: %s\n", strerror(errno));
    return -1;
  }

  char *s = (char*)malloc(message_size);
  if (s == NULL) {
    printf("error in malloc: %s\n", strerror(errno));
    return -1;
  }
  
  nmq::node_t node(context, 1);
  struct timeval start, stop;
  gettimeofday(&start, NULL);
  for (int i = 0; i != roundtrip_count; i++) {
    node.send(0, s, message_size);
  }
  gettimeofday(&stop, NULL);
  long delta = (stop.tv_sec - start.tv_sec)*1000000 + (stop.tv_usec - start.tv_usec);
  long thr = (roundtrip_count * 1000000) / delta;
  printf("%li msg/s\n", thr);  
}
