// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A test application for the RLZ library.
//
// These tests should not be executed on the build server:
// - They assert for the failed cases.
// - They modify machine state (registry).
//
// These tests require write access to HKLM and HKCU.
//
// The "GGLA" brand is used to test the normal code flow of the code, and the
// "TEST" brand is used to test the supplementary brand code code flow.

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "rlz/lib/financial_ping.h"
#include "rlz/lib/net_response_check.h"
#include "rlz/lib/rlz_lib.h"
#include "rlz/lib/rlz_value_store.h"
#include "rlz/test/rlz_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <Windows.h>
#include "rlz/win/lib/machine_deal.h"
#endif

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/threading/thread.h"
#include "net/url_request/url_request_test_util.h"
#endif


class MachineDealCodeHelper
#if defined(OS_WIN)
    : public rlz_lib::MachineDealCode
#endif
    {
 public:
  static bool Clear() {
#if defined(OS_WIN)
    return rlz_lib::MachineDealCode::Clear();
#else
    return true;
#endif
  }

 private:
  MachineDealCodeHelper() {}
  ~MachineDealCodeHelper() {}
};

class RlzLibTest : public RlzLibTestBase {
};

TEST_F(RlzLibTest, RecordProductEvent) {
  char cgi_50[50];

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S", cgi_50);

  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S,W1I", cgi_50);

  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S,W1I", cgi_50);
}

TEST_F(RlzLibTest, ClearProductEvent) {
  char cgi_50[50];

  // Clear 1 of 1 events.
  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S", cgi_50);
  EXPECT_TRUE(rlz_lib::ClearProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              cgi_50, 50));
  EXPECT_STREQ("", cgi_50);

  // Clear 1 of 2 events.
  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S,W1I", cgi_50);
  EXPECT_TRUE(rlz_lib::ClearProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=W1I", cgi_50);

  // Clear a non-recorded event.
  EXPECT_TRUE(rlz_lib::ClearProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IETB_SEARCH_BOX, rlz_lib::FIRST_SEARCH));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=W1I", cgi_50);
}


TEST_F(RlzLibTest, GetProductEventsAsCgi) {
  char cgi_50[50];
  char cgi_1[1];

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              cgi_1, 1));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S,W1I", cgi_50);
}

TEST_F(RlzLibTest, ClearAllAllProductEvents) {
  char cgi_50[50];

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S", cgi_50);

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              cgi_50, 50));
  EXPECT_STREQ("", cgi_50);
}

TEST_F(RlzLibTest, SetAccessPointRlz) {
  char rlz_50[50];
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, ""));
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_50, 50));
  EXPECT_STREQ("", rlz_50);

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, "IeTbRlz"));
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_50, 50));
  EXPECT_STREQ("IeTbRlz", rlz_50);
}

TEST_F(RlzLibTest, GetAccessPointRlz) {
  char rlz_1[1];
  char rlz_50[50];
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, ""));
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_1, 1));
  EXPECT_STREQ("", rlz_1);

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, "IeTbRlz"));
  EXPECT_FALSE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_1, 1));
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_50, 50));
  EXPECT_STREQ("IeTbRlz", rlz_50);
}

TEST_F(RlzLibTest, GetPingParams) {
  MachineDealCodeHelper::Clear();

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX,
      "TbRlzValue"));
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IE_HOME_PAGE, ""));

  char cgi[2048];
  rlz_lib::AccessPoint points[] =
    {rlz_lib::IETB_SEARCH_BOX, rlz_lib::NO_ACCESS_POINT,
     rlz_lib::NO_ACCESS_POINT};

  EXPECT_TRUE(rlz_lib::GetPingParams(rlz_lib::TOOLBAR_NOTIFIER, points,
                                     cgi, 2048));
  EXPECT_STREQ("rep=2&rlz=T4:TbRlzValue", cgi);

#if defined(OS_WIN)
  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));
#define DCC_PARAM "&dcc=dcc_value"
#else
#define DCC_PARAM ""
#endif

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, ""));
  EXPECT_TRUE(rlz_lib::GetPingParams(rlz_lib::TOOLBAR_NOTIFIER, points,
                                     cgi, 2048));
  EXPECT_STREQ("rep=2&rlz=T4:" DCC_PARAM, cgi);

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX,
              "TbRlzValue"));
  EXPECT_FALSE(rlz_lib::GetPingParams(rlz_lib::TOOLBAR_NOTIFIER, points,
                                      cgi, 23 + strlen(DCC_PARAM)));
  EXPECT_STREQ("", cgi);
  EXPECT_TRUE(rlz_lib::GetPingParams(rlz_lib::TOOLBAR_NOTIFIER, points,
                                     cgi, 24 + strlen(DCC_PARAM)));
  EXPECT_STREQ("rep=2&rlz=T4:TbRlzValue" DCC_PARAM, cgi);

  EXPECT_TRUE(GetAccessPointRlz(rlz_lib::IE_HOME_PAGE, cgi, 2048));
  points[2] = rlz_lib::IE_HOME_PAGE;
  EXPECT_TRUE(rlz_lib::GetPingParams(rlz_lib::TOOLBAR_NOTIFIER, points,
                                     cgi, 2048));
  EXPECT_STREQ("rep=2&rlz=T4:TbRlzValue" DCC_PARAM, cgi);
}

TEST_F(RlzLibTest, IsPingResponseValid) {
  const char* kBadPingResponses[] = {
    // No checksum.
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlz: 1R1_____en__252\r\n"
    "rlzXX: 1R1_____en__250\r\n",

    // Invalid checksum.
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlz: 1R1_____en__252\r\n"
    "rlzXX: 1R1_____en__250\r\n"
    "rlzT4  1T4_____en__251\r\n"
    "rlzT4: 1T4_____en__252\r\n"
    "rlz\r\n"
    "crc32: B12CC79A",

    // Misplaced checksum.
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlz: 1R1_____en__252\r\n"
    "rlzXX: 1R1_____en__250\r\n"
    "crc32: B12CC79C\r\n"
    "rlzT4  1T4_____en__251\r\n"
    "rlzT4: 1T4_____en__252\r\n"
    "rlz\r\n",

    NULL
  };

  const char* kGoodPingResponses[] = {
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlz: 1R1_____en__252\r\n"
    "rlzXX: 1R1_____en__250\r\n"
    "rlzT4  1T4_____en__251\r\n"
    "rlzT4: 1T4_____en__252\r\n"
    "rlz\r\n"
    "crc32: D6FD55A3",

    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlz: 1R1_____en__252\r\n"
    "rlzXX: 1R1_____en__250\r\n"
    "rlzT4  1T4_____en__251\r\n"
    "rlzT4: 1T4_____en__252\r\n"
    "rlz\r\n"
    "crc32: D6FD55A3\r\n"
    "extradata: not checksummed",

    NULL
  };

  for (int i = 0; kBadPingResponses[i]; i++)
    EXPECT_FALSE(rlz_lib::IsPingResponseValid(kBadPingResponses[i], NULL));

  for (int i = 0; kGoodPingResponses[i]; i++)
    EXPECT_TRUE(rlz_lib::IsPingResponseValid(kGoodPingResponses[i], NULL));
}

TEST_F(RlzLibTest, ParsePingResponse) {
  const char* kPingResponse =
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlz: 1R1_____en__252\r\n"  // Invalid RLZ - no access point.
    "rlzXX: 1R1_____en__250\r\n"  // Invalid RLZ - bad access point.
    "rlzT4  1T4_____en__251\r\n"  // Invalid RLZ - missing colon.
    "rlzT4: 1T4_____en__252\r\n"  // GoodRLZ.
    "events: I7S,W1I\r\n"  // Clear all events.
    "rlz\r\n"
    "dcc: dcc_value\r\n"
    "crc32: F9070F81";

#if defined(OS_WIN)
  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value2"));
#endif

  // Record some product events to check that they get cleared.
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(
      rlz_lib::IETB_SEARCH_BOX, "TbRlzValue"));

  EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                         kPingResponse));

#if defined(OS_WIN)
  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));
#endif
  EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                         kPingResponse));

  char value[50];
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, value, 50));
  EXPECT_STREQ("1T4_____en__252", value);
  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              value, 50));
  EXPECT_STREQ("", value);

  const char* kPingResponse2 =
    "rlzT4:    1T4_____de__253  \r\n"  // Good with extra spaces.
    "crc32: 321334F5\r\n";
  EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                         kPingResponse2));
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, value, 50));
  EXPECT_STREQ("1T4_____de__253", value);

  const char* kPingResponse3 =
    "crc32: 0\r\n";  // Good RLZ - empty response.
  EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                         kPingResponse3));
  EXPECT_STREQ("1T4_____de__253", value);
}

// Test whether a stateful event will only be sent in financial pings once.
TEST_F(RlzLibTest, ParsePingResponseWithStatefulEvents) {
  const char* kPingResponse =
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlzT4: 1T4_____en__252\r\n"  // GoodRLZ.
    "events: I7S,W1I\r\n"         // Clear all events.
    "stateful-events: W1I\r\n"    // W1I as an stateful event.
    "rlz\r\n"
    "dcc: dcc_value\r\n"
    "crc32: 55191759";

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));

  // Record some product events to check that they get cleared.
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(
      rlz_lib::IETB_SEARCH_BOX, "TbRlzValue"));

  EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                         kPingResponse));

  // Check all the events sent earlier are cleared.
  char value[50];
  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              value, 50));
  EXPECT_STREQ("", value);

  // Record both events (one is stateless and the other is stateful) again.
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  // Check the stateful event won't be sent again while the stateless one will.
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             value, 50));
  EXPECT_STREQ("events=I7S", value);

  // Test that stateful events are cleared by ClearAllProductEvents().  After
  // calling it, trying to record a stateful again should result in it being
  // recorded again.
  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             value, 50));
  EXPECT_STREQ("events=W1I", value);
}

class URLRequestRAII {
 public:
  URLRequestRAII(net::URLRequestContextGetter* context) {
    rlz_lib::SetURLRequestContext(context);
  }
  ~URLRequestRAII() {
    rlz_lib::SetURLRequestContext(NULL);
  }
};

TEST_F(RlzLibTest, SendFinancialPing) {
  // We don't really check a value or result in this test. All this does is
  // attempt to ping the financial server, which you can verify in Fiddler.
  // TODO: Make this a measurable test.

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;

  base::Thread io_thread("rlz_unittest_io_thread");
  ASSERT_TRUE(io_thread.StartWithOptions(options));

  scoped_refptr<net::TestURLRequestContextGetter> context =
      new net::TestURLRequestContextGetter(io_thread.task_runner());
  rlz_lib::SetURLRequestContext(context.get());

  URLRequestRAII set_context(context.get());
#endif

  MachineDealCodeHelper::Clear();
#if defined(OS_WIN)
  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));
#endif

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX,
      "TbRlzValue"));

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  rlz_lib::AccessPoint points[] =
    {rlz_lib::IETB_SEARCH_BOX, rlz_lib::NO_ACCESS_POINT,
     rlz_lib::NO_ACCESS_POINT};

  std::string request;
  rlz_lib::SendFinancialPing(rlz_lib::TOOLBAR_NOTIFIER, points,
      "swg", "GGLA", "SwgProductId1234", "en-UK", false,
      /*skip_time_check=*/true);
}

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)

void ResetContext() {
  rlz_lib::SetURLRequestContext(NULL);
}

TEST_F(RlzLibTest, SendFinancialPingDuringShutdown) {
  // rlz_lib::SendFinancialPing fails when this is set.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;

  base::Thread io_thread("rlz_unittest_io_thread");
  ASSERT_TRUE(io_thread.StartWithOptions(options));

  scoped_refptr<net::TestURLRequestContextGetter> context =
      new net::TestURLRequestContextGetter(io_thread.task_runner());
  rlz_lib::SetURLRequestContext(context.get());

  URLRequestRAII set_context(context.get());

  rlz_lib::AccessPoint points[] =
    {rlz_lib::IETB_SEARCH_BOX, rlz_lib::NO_ACCESS_POINT,
     rlz_lib::NO_ACCESS_POINT};
  rlz_lib::test::ResetSendFinancialPingInterrupted();
  EXPECT_FALSE(rlz_lib::test::WasSendFinancialPingInterrupted());

  base::MessageLoop loop;
  loop.task_runner()->PostTask(FROM_HERE, base::Bind(&ResetContext));
  std::string request;
  EXPECT_FALSE(rlz_lib::SendFinancialPing(rlz_lib::TOOLBAR_NOTIFIER, points,
      "swg", "GGLA", "SwgProductId1234", "en-UK", false,
      /*skip_time_check=*/true));

  EXPECT_TRUE(rlz_lib::test::WasSendFinancialPingInterrupted());
  rlz_lib::test::ResetSendFinancialPingInterrupted();
}
#endif

TEST_F(RlzLibTest, ClearProductState) {
  MachineDealCodeHelper::Clear();

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX,
      "TbRlzValue"));
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::GD_DESKBAND,
      "GdbRlzValue"));

  rlz_lib::AccessPoint points[] =
      { rlz_lib::IETB_SEARCH_BOX, rlz_lib::NO_ACCESS_POINT };

  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IETB_SEARCH_BOX, rlz_lib::INSTALL));

  rlz_lib::AccessPoint points2[] =
    { rlz_lib::IETB_SEARCH_BOX,
      rlz_lib::GD_DESKBAND,
      rlz_lib::NO_ACCESS_POINT };

  char cgi[2048];
  EXPECT_TRUE(rlz_lib::GetPingParams(rlz_lib::TOOLBAR_NOTIFIER, points2,
                                     cgi, 2048));
  EXPECT_STREQ("rep=2&rlz=T4:TbRlzValue,D1:GdbRlzValue", cgi);

  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi, 2048));
  std::string events(cgi);
  EXPECT_LT(0u, events.find("I7S"));
  EXPECT_LT(0u, events.find("T4I"));
  EXPECT_LT(0u, events.find("T4R"));

  rlz_lib::ClearProductState(rlz_lib::TOOLBAR_NOTIFIER, points);

  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX,
                                         cgi, 2048));
  EXPECT_STREQ("", cgi);
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::GD_DESKBAND,
                                         cgi, 2048));
  EXPECT_STREQ("GdbRlzValue", cgi);

  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              cgi, 2048));
  EXPECT_STREQ("", cgi);
}

#if defined(OS_WIN)
template<class T>
class typed_buffer_ptr {
  std::unique_ptr<char[]> buffer_;

 public:
  typed_buffer_ptr() {
  }

  explicit typed_buffer_ptr(size_t size) : buffer_(new char[size]) {
  }

  void reset(size_t size) {
    buffer_.reset(new char[size]);
  }

  operator T*() {
    return reinterpret_cast<T*>(buffer_.get());
  }
};

namespace rlz_lib {
bool HasAccess(PSID sid, ACCESS_MASK access_mask, ACL* dacl);
}

bool EmptyAcl(ACL* acl) {
  ACL_SIZE_INFORMATION info;
  bool ret = GetAclInformation(acl, &info, sizeof(info), AclSizeInformation);
  EXPECT_TRUE(ret);

  for (DWORD i = 0; i < info.AceCount && ret; ++i) {
    ret = DeleteAce(acl, 0);
    EXPECT_TRUE(ret);
  }

  return ret;
}

TEST_F(RlzLibTest, HasAccess) {
  // Create a SID that represents ALL USERS.
  DWORD users_sid_size = SECURITY_MAX_SID_SIZE;
  typed_buffer_ptr<SID> users_sid(users_sid_size);
  CreateWellKnownSid(WinBuiltinUsersSid, NULL, users_sid, &users_sid_size);

  // RLZ always asks for KEY_ALL_ACCESS access to the key.  This is what we
  // test here.

  // No ACL mean no access.
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, NULL));

  // Create an ACL for these tests.
  const DWORD kMaxAclSize = 1024;
  typed_buffer_ptr<ACL> dacl(kMaxAclSize);
  InitializeAcl(dacl, kMaxAclSize, ACL_REVISION);

  // Empty DACL mean no access.
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // ACE without all needed privileges should mean no access.
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_READ, users_sid));
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // ACE without all needed privileges should mean no access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_WRITE, users_sid));
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // A deny ACE before an allow ACE should not give access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessDeniedAce(dacl, ACL_REVISION, KEY_ALL_ACCESS,
                                 users_sid));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_ALL_ACCESS,
                                  users_sid));
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // A deny ACE before an allow ACE should not give access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessDeniedAce(dacl, ACL_REVISION, KEY_READ, users_sid));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_ALL_ACCESS,
                                  users_sid));
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));


  // An allow ACE without all required bits should not give access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_WRITE, users_sid));
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // An allow ACE with all required bits should give access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_ALL_ACCESS,
                                  users_sid));
  EXPECT_TRUE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // A deny ACE after an allow ACE should not give access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_ALL_ACCESS,
                                  users_sid));
  EXPECT_TRUE(AddAccessDeniedAce(dacl, ACL_REVISION, KEY_READ, users_sid));
  EXPECT_TRUE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // An inherit-only allow ACE should not give access.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessAllowedAceEx(dacl, ACL_REVISION, INHERIT_ONLY_ACE,
                                    KEY_ALL_ACCESS, users_sid));
  EXPECT_FALSE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));

  // An inherit-only deny ACE should not apply.
  EXPECT_TRUE(EmptyAcl(dacl));
  EXPECT_TRUE(AddAccessDeniedAceEx(dacl, ACL_REVISION, INHERIT_ONLY_ACE,
                                   KEY_ALL_ACCESS, users_sid));
  EXPECT_TRUE(AddAccessAllowedAce(dacl, ACL_REVISION, KEY_ALL_ACCESS,
                                  users_sid));
  EXPECT_TRUE(rlz_lib::HasAccess(users_sid, KEY_ALL_ACCESS, dacl));
}
#endif

TEST_F(RlzLibTest, BrandingRecordProductEvent) {
  // Don't run these tests if a supplementary brand is already in place.  That
  // way we can control the branding.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  char cgi_50[50];

  // Record different events for the same product with diffrent branding, and
  // make sure that the information remains separate.
  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  {
    rlz_lib::SupplementaryBranding branding("TEST");
    EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  }

  // Test that recording events with the default brand and a supplementary
  // brand don't overwrite each other.

  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S", cgi_50);

  {
    rlz_lib::SupplementaryBranding branding("TEST");
    EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
        rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::INSTALL));
    EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                               cgi_50, 50));
    EXPECT_STREQ("events=I7I", cgi_50);
  }

  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             cgi_50, 50));
  EXPECT_STREQ("events=I7S", cgi_50);
}

TEST_F(RlzLibTest, BrandingSetAccessPointRlz) {
  // Don't run these tests if a supplementary brand is already in place.  That
  // way we can control the branding.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  char rlz_50[50];

  // Test that setting RLZ strings with the default brand and a supplementary
  // brand don't overwrite each other.

  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, "IeTbRlz"));
  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_50, 50));
  EXPECT_STREQ("IeTbRlz", rlz_50);

  {
    rlz_lib::SupplementaryBranding branding("TEST");

    EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, "SuppRlz"));
    EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_50,
                                           50));
    EXPECT_STREQ("SuppRlz", rlz_50);
  }

  EXPECT_TRUE(rlz_lib::GetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, rlz_50, 50));
  EXPECT_STREQ("IeTbRlz", rlz_50);

}

TEST_F(RlzLibTest, BrandingWithStatefulEvents) {
  // Don't run these tests if a supplementary brand is already in place.  That
  // way we can control the branding.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  const char* kPingResponse =
    "version: 3.0.914.7250\r\n"
    "url: http://www.corp.google.com/~av/45/opt/SearchWithGoogleUpdate.exe\r\n"
    "launch-action: custom-action\r\n"
    "launch-target: SearchWithGoogleUpdate.exe\r\n"
    "signature: c08a3f4438e1442c4fe5678ee147cf6c5516e5d62bb64e\r\n"
    "rlzT4: 1T4_____en__252\r\n"  // GoodRLZ.
    "events: I7S,W1I\r\n"         // Clear all events.
    "stateful-events: W1I\r\n"    // W1I as an stateful event.
    "rlz\r\n"
    "dcc: dcc_value\r\n"
    "crc32: 55191759";

  EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  {
    rlz_lib::SupplementaryBranding branding("TEST");
    EXPECT_TRUE(rlz_lib::ClearAllProductEvents(rlz_lib::TOOLBAR_NOTIFIER));
  }

  // Record some product events for the default and supplementary brand.
  // Check that they get cleared only for the default brand.
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  {
    rlz_lib::SupplementaryBranding branding("TEST");
    EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
        rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
    EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
        rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));
  }

  EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                         kPingResponse));

  // Check all the events sent earlier are cleared only for default brand.
  char value[50];
  EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                              value, 50));
  EXPECT_STREQ("", value);

  {
    rlz_lib::SupplementaryBranding branding("TEST");
    EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                               value, 50));
    EXPECT_STREQ("events=I7S,W1I", value);
  }

  // Record both events (one is stateless and the other is stateful) again.
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_HOME_PAGE, rlz_lib::INSTALL));

  // Check the stateful event won't be sent again while the stateless one will.
  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             value, 50));
  EXPECT_STREQ("events=I7S", value);

  {
    rlz_lib::SupplementaryBranding branding("TEST");
    EXPECT_TRUE(rlz_lib::ParsePingResponse(rlz_lib::TOOLBAR_NOTIFIER,
                                           kPingResponse));

    EXPECT_FALSE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                                value, 50));
    EXPECT_STREQ("", value);
  }

  EXPECT_TRUE(rlz_lib::GetProductEventsAsCgi(rlz_lib::TOOLBAR_NOTIFIER,
                                             value, 50));
  EXPECT_STREQ("events=I7S", value);
}

#if defined(OS_POSIX)
class ReadonlyRlzDirectoryTest : public RlzLibTestNoMachineState {
 protected:
  void SetUp() override;
};

void ReadonlyRlzDirectoryTest::SetUp() {
  RlzLibTestNoMachineState::SetUp();
  // Make the rlz directory non-writeable.
  int chmod_result =
      chmod(m_rlz_test_helper_.temp_dir_.GetPath().value().c_str(), 0500);
  ASSERT_EQ(0, chmod_result);
}

TEST_F(ReadonlyRlzDirectoryTest, WriteFails) {
  // The rlz test runner runs every test twice: Once normally, and once with
  // a SupplementaryBranding on the stack. In the latter case, the rlz lock
  // has already been acquired before the rlz directory got changed to
  // read-only, which makes this test pointless. So run it only in the first
  // pass.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  EXPECT_FALSE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::SET_TO_GOOGLE));
}

// Regression test for http://crbug.com/121255
TEST_F(ReadonlyRlzDirectoryTest, SupplementaryBrandingDoesNotCrash) {
  // See the comment at the top of WriteFails.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  rlz_lib::SupplementaryBranding branding("TEST");
  EXPECT_FALSE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::INSTALL));
}

// Regression test for http://crbug.com/141108
TEST_F(RlzLibTest, ConcurrentStoreAccessWithProcessExitsWhileLockHeld) {
  // See the comment at the top of WriteFails.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  std::vector<pid_t> pids;
  for (int i = 0; i < 10; ++i) {
    pid_t pid = fork();
    ASSERT_NE(-1, pid);
    if (pid == 0) {
      // Child.
      {
        // SupplementaryBranding is a RAII object for the rlz lock.
        rlz_lib::SupplementaryBranding branding("TEST");

        // Simulate a crash while holding the lock in some of the children.
        if (i > 0 && i % 3 == 0)
          _exit(0);

        // Note: Since this is in a forked child, a failing expectation won't
        // make the test fail. It does however cause lots of "check failed"
        // error output. The parent process will then check the exit code
        // below to make the test fail.
        bool success = rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
            rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::INSTALL);
        EXPECT_TRUE(success);
        _exit(success ? 0 : 1);
      }
      _exit(0);
    } else {
      // Parent.
      pids.push_back(pid);
    }
  }

  int status;
  for (size_t i = 0; i < pids.size(); ++i) {
    if (HANDLE_EINTR(waitpid(pids[i], &status, 0)) != -1)
      EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }

  // No child should have the lock at this point, not even the crashed ones.
  EXPECT_TRUE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::INSTALL));
}

TEST_F(RlzLibTest, LockAcquistionSucceedsButStoreFileCannotBeCreated) {
  // See the comment at the top of WriteFails.
  if (!rlz_lib::SupplementaryBranding::GetBrand().empty())
    return;

  // Create a directory where the rlz file is supposed to appear. This way,
  // the lock file can be created successfully, but creation of the rlz file
  // itself will fail.
  int mkdir_result = mkdir(rlz_lib::testing::RlzStoreFilenameStr().c_str(),
                           0500);
  ASSERT_EQ(0, mkdir_result);

  rlz_lib::SupplementaryBranding branding("TEST");
  EXPECT_FALSE(rlz_lib::RecordProductEvent(rlz_lib::TOOLBAR_NOTIFIER,
      rlz_lib::IE_DEFAULT_SEARCH, rlz_lib::INSTALL));
}

#endif
