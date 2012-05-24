// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <string>

#include "base/message_loop.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

TEST(WebkitGlueTest, DecodeImageFail) {
  std::string data("not an image");
  SkBitmap image;
  EXPECT_FALSE(webkit_glue::DecodeImage(data, &image));
  EXPECT_TRUE(image.isNull());
}

TEST(WebkitGlueTest, DecodeImage) {
  std::string data("GIF87a\x02\x00\x02\x00\xa1\x04\x00\x00\x00\x00\x00\x00\xff"
                   "\xff\x00\x00\x00\xff\x00,\x00\x00\x00\x00\x02\x00\x02\x00"
                   "\x00\x02\x03\x84\x16\x05\x00;", 42);
  EXPECT_EQ(42u, data.size());
  SkBitmap image;
  EXPECT_TRUE(webkit_glue::DecodeImage(data, &image));
  EXPECT_FALSE(image.isNull());
  EXPECT_EQ(2, image.width());
  EXPECT_EQ(2, image.height());
  EXPECT_EQ(SkBitmap::kARGB_8888_Config, image.config());
  image.lockPixels();
  EXPECT_EQ(SK_ColorBLACK, *image.getAddr32(0, 0));
  EXPECT_EQ(SK_ColorRED, *image.getAddr32(1, 0));
  EXPECT_EQ(SK_ColorGREEN, *image.getAddr32(0, 1));
  EXPECT_EQ(SK_ColorBLUE, *image.getAddr32(1, 1));
  image.unlockPixels();
}

class WebkitGlueUserAgentTest : public TestShellTest {
};

bool IsSpoofedUserAgent(const std::string& user_agent) {
  return user_agent.find("TestShell") == std::string::npos;
}

TEST_F(WebkitGlueUserAgentTest, UserAgentSpoofingHack) {
  enum Platform {
    NONE = 0,
    MACOSX = 1,
    WIN = 2,
    OTHER = 4,
  };

  struct Expected {
    const char* url;
    int os_mask;
  };

  Expected expected[] = {
      { "http://wwww.google.com", NONE },
      { "http://www.microsoft.com/getsilverlight", MACOSX },
      { "http://headlines.yahoo.co.jp/videonews/", MACOSX | WIN },
      { "http://downloads.yahoo.co.jp/docs/silverlight/", MACOSX },
      { "http://gyao.yahoo.co.jp/", MACOSX },
      { "http://weather.yahoo.co.jp/weather/zoomradar/", WIN },
      { "http://promotion.shopping.yahoo.co.jp/", WIN },
      { "http://pokemon.kids.yahoo.co.jp", WIN },
  };
#if defined(OS_MACOSX)
  int os_bit = MACOSX;
#elif defined(OS_WIN)
  int os_bit = WIN;
#else
  int os_bit = OTHER;
#endif

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected); ++i) {
    EXPECT_EQ((expected[i].os_mask & os_bit) != 0,
              IsSpoofedUserAgent(
                  webkit_glue::GetUserAgent(GURL(expected[i].url))));
  }
}

// Derives WebKitPlatformSupportImpl for testing shared timers.
class TestWebKitPlatformSupport
    : public webkit_glue::WebKitPlatformSupportImpl {
 public:
  TestWebKitPlatformSupport() : mock_monotonically_increasing_time_(0) {
  }

  // WebKitPlatformSupportImpl implementation
  virtual string16 GetLocalizedString(int) OVERRIDE {
    return string16();
  }

  virtual base::StringPiece GetDataResource(int, ui::ScaleFactor) OVERRIDE {
    return base::StringPiece();
  }

  virtual void GetPlugins(bool,
                          std::vector<webkit::WebPluginInfo,
                          std::allocator<webkit::WebPluginInfo> >*) OVERRIDE {
  }

  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo&) OVERRIDE {
    return NULL;
  }

  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle*,
      webkit_glue::WebSocketStreamHandleDelegate*) OVERRIDE {
    return NULL;
  }

  // Returns mock time when enabled.
  virtual double monotonicallyIncreasingTime() OVERRIDE {
    if (mock_monotonically_increasing_time_ > 0.0)
      return mock_monotonically_increasing_time_;
    return webkit_glue::WebKitPlatformSupportImpl::
        monotonicallyIncreasingTime();
  }

  virtual void OnStartSharedTimer(base::TimeDelta delay) OVERRIDE {
    shared_timer_delay_ = delay;
  }

  base::TimeDelta shared_timer_delay() {
    return shared_timer_delay_;
  }

  void set_mock_monotonically_increasing_time(double mock_time) {
    mock_monotonically_increasing_time_ = mock_time;
  }

 private:
  base::TimeDelta shared_timer_delay_;
  double mock_monotonically_increasing_time_;
};

TEST(WebkitGlueTest, SuspendResumeSharedTimer) {
  TestWebKitPlatformSupport platform_support;

  // Set a timer to fire as soon as possible.
  platform_support.setSharedTimerFireInterval(0);
  // Suspend timers immediately so the above timer wouldn't be fired.
  platform_support.SuspendSharedTimer();
  // The above timer would have posted a task which can be processed out of the
  // message loop.
  MessageLoop::current()->RunAllPending();
  // Set a mock time after 1 second to simulate timers suspended for 1 second.
  double new_time = base::Time::Now().ToDoubleT() + 1;
  platform_support.set_mock_monotonically_increasing_time(new_time);
  // Resume timers so that the timer set above will be set again to fire
  // immediately.
  platform_support.ResumeSharedTimer();
  EXPECT_TRUE(base::TimeDelta() == platform_support.shared_timer_delay());
}

}  // namespace
