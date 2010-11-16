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

#include <gtest/gtest.h>
#include <nmq.hpp>

TEST(Tests, Test) {

  // Open context
  char *fname = tempnam(NULL, "nmq"); // UGLY
  ASSERT_NE(NULL, (size_t)fname);
  nmq::context_t context(fname);
  ASSERT_TRUE(context.create(4, 10, 100));

  nmq::node_t node0(context, 0);
  nmq::node_t node1(context, 1);
  nmq::node_t node2(context, 2);
  nmq::node_t node3(context, 3);

  char buf[100];
  size_t sz = 100;
  node0.send(1, "test", 5);
  node1.recv(0, &buf, &sz);
  EXPECT_EQ(5, sz);
  EXPECT_STREQ("test", buf);  

  node0.send(2, "test", 5);
  node2.recv(0, &buf, &sz);
  EXPECT_EQ(5, sz);
  EXPECT_STREQ("test", buf);  
  
  node0.send(3, "test", 5);
  node3.recv(0, &buf, &sz);
  EXPECT_EQ(5, sz);
  EXPECT_STREQ("test", buf);

  ASSERT_EQ(0, unlink(fname));
}


int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
