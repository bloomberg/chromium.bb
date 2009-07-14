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


#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"

#include "gtest/gtest.h"

namespace {

// set, get, setavail operations on the descriptor table
TEST(SelLdrTest, DescTable) {
  struct NaClApp app;
  struct NaClHostDesc *host_desc;
  struct NaClDesc* io_desc;
  struct NaClDesc* ret_desc;
  int ret_code;

  ret_code = NaClAppCtor(&app);
  ASSERT_EQ(1, ret_code);

  host_desc = (struct NaClHostDesc *)malloc(sizeof *host_desc);
  io_desc = (struct NaClDesc *) NaClDescIoDescMake(host_desc);

  // 1st pos available is 0
  ret_code = NaClSetAvail(&app, io_desc);
  ASSERT_EQ(0, ret_code);
  // valid desc at pos 0
  ret_desc = NaClGetDesc(&app, 0);
  ASSERT_TRUE(NULL != ret_desc);

  // next pos available is 1
  ret_code = NaClSetAvail(&app, NULL);
  ASSERT_EQ(1, ret_code);
  // no desc at pos 1
  ret_desc = NaClGetDesc(&app, 1);
  ASSERT_TRUE(NULL == ret_desc);

  // no desc at pos 1 -> pos 1 is available
  ret_code = NaClSetAvail(&app, io_desc);
  ASSERT_EQ(1, ret_code);
  // valid desc at pos 1
  ret_desc = NaClGetDesc(&app, 1);
  ASSERT_TRUE(NULL != ret_desc);

  // set no desc at pos 3
  NaClSetDesc(&app, 3, NULL);

  // valid desc at pos 4
  NaClSetDesc(&app, 4, io_desc);
  ret_desc = NaClGetDesc(&app, 4);
  ASSERT_TRUE(NULL != ret_desc);

  // never set a desc at pos 10
  ret_desc = NaClGetDesc(&app, 10);
  ASSERT_TRUE(NULL == ret_desc);

  NaClAppDtor(&app);
}

// create service socket
TEST(SelLdrTest, CreateServiceSocket) {
  struct NaClApp app;
  int ret_code;

  NaClNrdAllModulesInit();
  ret_code = NaClAppCtor(&app);
  ASSERT_EQ(1, ret_code);

  // CreateServiceSocket sets the app service_port to a service port
  // desc and service_address to a service
  ASSERT_TRUE(NULL == app.service_port);
  ASSERT_TRUE(NULL == app.service_address);
  NaClCreateServiceSocket(&app);
  ASSERT_TRUE(NULL != app.service_port);
  ASSERT_TRUE(NULL != app.service_address);

  NaClAppDtor(&app);
  NaClNrdAllModulesFini();
}

// add and remove operations on the threads table
// Remove thread from an empty table is tested in a death test.
// TODO(tuduce): specify the death test name when checking in.
TEST(SelLdrTest, ThreadTableTest) {
  struct NaClApp app;
  struct NaClAppThread nat, *appt=&nat;
  int ret_code;

  ret_code = NaClAppCtor(&app);
  ASSERT_EQ(1, ret_code);

  // 1st pos available is 0
  ASSERT_EQ(0, app.num_threads);
  ret_code = NaClAddThread(&app, appt);
  ASSERT_EQ(0, ret_code);
  ASSERT_EQ(1, app.num_threads);

  // next pos available is 1
  ret_code = NaClAddThread(&app, NULL);
  ASSERT_EQ(1, ret_code);
  ASSERT_EQ(2, app.num_threads);

  // no thread at pos 1 -> pos 1 is available
  ret_code = NaClAddThread(&app, appt);
  ASSERT_EQ(1, ret_code);
  ASSERT_EQ(3, app.num_threads);

  NaClRemoveThread(&app, 0);
  ASSERT_EQ(2, app.num_threads);

  // calling the Dtor results into segfault
//   NaClAppDtor(&app);
}
}
