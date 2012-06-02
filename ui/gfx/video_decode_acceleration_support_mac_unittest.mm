// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/video_decode_acceleration_support_mac.h"

#import "base/bind.h"
#include "base/location.h"
#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#import "base/sys_info.h"
#import "base/threading/platform_thread.h"
#import "base/threading/thread.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

// Aribtrary values used to test the callback.
const int kCallbackFrameID = 10;
const int kCallbackExpectedStatus = 7;

// Sample movie data to create the decoder.
const int kSampleWidth = 1280;
const int kSampleHeight = 720;
const uint8_t kSampleAVCData[] =  {
  0x1, 0x64, 0x0, 0x1f, 0xff, 0xe1, 0x0, 0x1a,
  0x67, 0x64, 0x0, 0x1f, 0xac, 0x2c, 0xc5, 0x1,
  0x40, 0x16, 0xec, 0x4, 0x40, 0x0, 0x0, 0x3,
  0x0, 0x40, 0x0, 0x0, 0xf, 0x23, 0xc6, 0xc,
  0x65, 0x80, 0x1, 0x0, 0x5, 0x68, 0xe9, 0x2b,
  0x2c, 0x8b,
};

// Check to see if the OS we're running on should have
// VideoDecodeAcceleration.framework installed.
bool OSShouldHaveFramework() {
  if (base::mac::IsOSLeopardOrEarlier())
    return false;

  // 10.6.2 and earlier doesn't have the framework.
  int32 major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  if (major == 10 && minor == 6 && bugfix <= 2)
    return false;

  return true;
}

// This function is provided as a callback for
// VideoDecodeAccelerationSupport::Decode. It verifies that callback
// arguments are correct and that the callback happened on the correct
// thread.
void FrameCallbackVerifier(bool* callback_done,
                           MessageLoop* expected_loop,
                           CVImageBufferRef image,
                           int status) {
  EXPECT_EQ(kCallbackExpectedStatus, status);
  // Verify that the callback was invoked on the same thread that created the
  // VDAStatus object.
  EXPECT_EQ(expected_loop, MessageLoop::current());
  *callback_done = true;
}

}  // namespace

namespace gfx {

class VideoDecodeAccelerationSupportTest : public ui::CocoaTest {
};

// Test that creating VideoDecodeAccelerationSupport works on hardware that
// supports it.
TEST_F(VideoDecodeAccelerationSupportTest, Create) {
  scoped_refptr<gfx::VideoDecodeAccelerationSupport> vda(
      new gfx::VideoDecodeAccelerationSupport);
  gfx::VideoDecodeAccelerationSupport::Status status = vda->Create(
      kSampleWidth, kSampleHeight, kCVPixelFormatType_422YpCbCr8,
      kSampleAVCData, arraysize(kSampleAVCData));

  // We should get an error loading the framework on 10.6.2 and earlier.
  if (!OSShouldHaveFramework()) {
    EXPECT_EQ(gfx::VideoDecodeAccelerationSupport::VDA_LOAD_FRAMEWORK_ERROR,
              status);
    return;
  }

  // If the hardware is not supported then there's not much we can do.
  if (status ==
      gfx::VideoDecodeAccelerationSupport::VDA_HARDWARE_NOT_SUPPORTED_ERROR) {
    return;
  }

  EXPECT_EQ(gfx::VideoDecodeAccelerationSupport::VDA_SUCCESS, status);
  EXPECT_EQ(gfx::VideoDecodeAccelerationSupport::VDA_SUCCESS,
            vda->Flush(false));
  EXPECT_EQ(gfx::VideoDecodeAccelerationSupport::VDA_SUCCESS, vda->Destroy());
}

// Test that callback works.
TEST_F(VideoDecodeAccelerationSupportTest, Callback) {
  MessageLoop loop;
  bool callback_done = false;

  scoped_refptr<gfx::VideoDecodeAccelerationSupport> vda(
      new gfx::VideoDecodeAccelerationSupport);
  vda->frame_ready_callbacks_[kCallbackFrameID] = base::Bind(
      &FrameCallbackVerifier, &callback_done, &loop);

  base::Thread thread("calllback");
  thread.Start();

  NSDictionary* info = [NSDictionary
      dictionaryWithObject:[NSNumber numberWithInt:kCallbackFrameID]
                    forKey:@"frame_id"];
  thread.message_loop()->PostTask(FROM_HERE, base::Bind(
      &gfx::VideoDecodeAccelerationSupport::OnFrameReadyCallback,
      static_cast<void*>(vda.get()),
      base::mac::NSToCFCast(info), kCallbackExpectedStatus, 0,
      static_cast<CVImageBufferRef>(NULL)));
  // Wait for the thread to complete.
  thread.Stop();

  // Verify that the callback occured.
  EXPECT_FALSE(callback_done);
  loop.RunAllPending();
  EXPECT_TRUE(callback_done);
}

}  // namespace gfx
