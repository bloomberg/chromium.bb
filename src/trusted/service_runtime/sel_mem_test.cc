/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/trusted/service_runtime/sel_mem.h"
#include "gtest/gtest.h"

namespace {

TEST(SelMemTest, AddTest) {
  struct NaClVmmap mem_map;
  int start_page_num = 32;
  int ret_code;

  ret_code = NaClVmmapCtor(&mem_map);
  EXPECT_EQ(1, ret_code);

  for (int i = 1; i <= 5; ++i) {
    ret_code = NaClVmmapAdd(&mem_map,
                            start_page_num*i,
                            i,
                            PROT_READ | PROT_EXEC,
                            (struct NaClMemObj *) NULL);
    EXPECT_EQ(1, ret_code);
    EXPECT_EQ(i, static_cast<int>(mem_map.nvalid));
    EXPECT_EQ(5, static_cast<int>(mem_map.size));
  }

  // no checks for start_page_num ..
  ret_code = NaClVmmapAdd(&mem_map,
                          start_page_num,
                          2,
                          PROT_READ,
                          (struct NaClMemObj *) NULL);
  EXPECT_EQ(6, static_cast<int>(mem_map.nvalid));
  EXPECT_EQ(10, static_cast<int>(mem_map.size));

  NaClVmmapDtor(&mem_map);
}

TEST(SelMemTest, UpdateTest) {
  struct NaClVmmap mem_map;

  NaClVmmapCtor(&mem_map);

  // 1st region
  NaClVmmapUpdate(&mem_map,
                  32,
                  12,
                  PROT_READ | PROT_EXEC,
                  (struct NaClMemObj *) NULL,
                  0);
  EXPECT_EQ(1, static_cast<int>(mem_map.nvalid));

  // no overlap
  NaClVmmapUpdate(&mem_map,
                  64,
                  10,
                  PROT_READ,
                  (struct NaClMemObj *) NULL,
                  0);
  // vmmap is [32, 44], [64, 74]
  EXPECT_EQ(2, static_cast<int>(mem_map.nvalid));

  // new mapping overlaps end and start of existing mappings
  NaClVmmapUpdate(&mem_map,
                  42,
                  24,
                  PROT_READ,
                  (struct NaClMemObj *) NULL,
                  0);
  // vmmap is [32, 41], [42, 66], [67, 74]
  EXPECT_EQ(3, static_cast<int>(mem_map.nvalid));

  // new mapping is in the middle of existing mapping
  NaClVmmapUpdate(&mem_map,
                  36,
                  2,
                  PROT_READ | PROT_EXEC,
                  (struct NaClMemObj *) NULL,
                  0);
  // vmmap is [32, 35], [34, 36], [37, 41], [42, 66], [67, 74]
  EXPECT_EQ(5, static_cast<int>(mem_map.nvalid));

  // new mapping covers all of the existing mapping
  NaClVmmapUpdate(&mem_map,
                  32,
                  6,
                  PROT_READ | PROT_EXEC,
                  (struct NaClMemObj *) NULL,
                  0);
  // vmmap is [32, 36], [37, 41], [42, 66], [67, 74]
  EXPECT_EQ(4, static_cast<int>(mem_map.nvalid));

  // remove existing mappings
  NaClVmmapUpdate(&mem_map,
                  40,
                  30,
                  PROT_READ | PROT_EXEC,
                  (struct NaClMemObj *) NULL,
                  1);
  // vmmap is [32, 36], [37, 39], [71, 74]
  EXPECT_EQ(3, static_cast<int>(mem_map.nvalid));

  NaClVmmapDtor(&mem_map);
}

TEST(SelMemTest, FindPageTest) {
  struct NaClVmmap mem_map;
  int ret_code;

  ret_code = NaClVmmapCtor(&mem_map);
  EXPECT_EQ(1, ret_code);

  struct NaClVmmapEntry const *entry;
  entry = NaClVmmapFindPage(&mem_map, 32);
  EXPECT_TRUE(NULL == entry);

  int start_page_num = 32;
  for (int i = 1; i <= 6; ++i) {
    ret_code = NaClVmmapAdd(&mem_map,
                            start_page_num*i,
                            2*i,
                            PROT_READ | PROT_EXEC,
                            (struct NaClMemObj *) NULL);
    EXPECT_EQ(1, ret_code);
    EXPECT_EQ(i, static_cast<int>(mem_map.nvalid));
  }
  // vmmap is [32, 34], [64, 68], [96, 102], [128, 136],
  //          [160, 170], [192, 204]

  entry = NaClVmmapFindPage(&mem_map, 16);
  EXPECT_TRUE(NULL == entry);

  entry = NaClVmmapFindPage(&mem_map, 32);
  EXPECT_TRUE(NULL != entry);

  entry = NaClVmmapFindPage(&mem_map, 34);
  EXPECT_TRUE(NULL == entry);

  entry = NaClVmmapFindPage(&mem_map, 202);
  EXPECT_TRUE(NULL != entry);

  NaClVmmapDtor(&mem_map);
}

TEST(SelMemTest, FindSpaceTest) {
  struct NaClVmmap mem_map;
  int ret_code;

  ret_code = NaClVmmapCtor(&mem_map);
  EXPECT_EQ(1, ret_code);

  // no entry
  ret_code = NaClVmmapFindSpace(&mem_map, 32);
  EXPECT_EQ(0, ret_code);

  NaClVmmapAdd(&mem_map,
               32,
               10,
               PROT_READ | PROT_EXEC,
               (struct NaClMemObj *) NULL);
  EXPECT_EQ(1, static_cast<int>(mem_map.nvalid));
  // one entry only
  ret_code = NaClVmmapFindSpace(&mem_map, 2);
  EXPECT_EQ(0, ret_code);

  NaClVmmapAdd(&mem_map,
               64,
               10,
               PROT_READ | PROT_EXEC,
               (struct NaClMemObj *) NULL);
  EXPECT_EQ(2, static_cast<int>(mem_map.nvalid));

  // the space is [32, 42], [64, 74]
  ret_code = NaClVmmapFindSpace(&mem_map, 32);
  EXPECT_EQ(0, ret_code);

  ret_code = NaClVmmapFindSpace(&mem_map, 2);
  EXPECT_EQ(62, ret_code);

  NaClVmmapAdd(&mem_map,
               96,
               10,
               PROT_READ | PROT_EXEC,
               (struct NaClMemObj *) NULL);
  EXPECT_EQ(3, static_cast<int>(mem_map.nvalid));

  // vmmap is [32, 42], [64, 74], [96, 106]
  // the search is from high address down
  ret_code = NaClVmmapFindSpace(&mem_map, 22);
  EXPECT_EQ(74, ret_code);

  NaClVmmapDtor(&mem_map);
}
}
