// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_VIDEO_DECODE_ACCELERATION_SUPPORT_MAC_H_
#define UI_GFX_VIDEO_DECODE_ACCELERATION_SUPPORT_MAC_H_
#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <map>

#if defined(MAC_OS_X_VERSION_10_6) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
#include <VideoDecodeAcceleration/VDADecoder.h>
#endif

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "ui/base/ui_export.h"

class FrameCallbackUtil;
class VideoDecodeAccelerationSupportTest;

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
typedef struct OpaqueVDADecoder*  VDADecoder;
#endif

namespace gfx {

// This Mac OS X-specific class provides dynamically linked access to
// VideoDecodeAcceleration.framework, which is only available on 10.6 and later.
class UI_EXPORT VideoDecodeAccelerationSupport :
    public base::RefCountedThreadSafe<VideoDecodeAccelerationSupport> {
 public:
  enum Status {
    VDA_SUCCESS = 0,
    VDA_LOAD_FRAMEWORK_ERROR,
    VDA_HARDWARE_NOT_SUPPORTED_ERROR,
    VDA_OTHER_ERROR,
  };

  typedef base::Callback<void(CVImageBufferRef, int status)> FrameReadyCallback;

  VideoDecodeAccelerationSupport();

  // Calls the create function.
  Status Create(int width, int height,
                int pixel_format,
                const void* avc_bytes, size_t avc_size);

  // Calls the decode function with the given data. |cb| is invoked once the
  // frame has been decoded.
  Status Decode(const void* bytes, size_t size, const FrameReadyCallback& cb);

  // Calls the flush function. If |emit_frames| is true then all pending frames
  // are synchronoulsy decoded, otherwise the pending frames are discarded.
  Status Flush(bool emit_frames);

  // Calls the destory function.
  Status Destroy();

 private:
  friend class base::RefCountedThreadSafe<VideoDecodeAccelerationSupport>;
  FRIEND_TEST_ALL_PREFIXES(VideoDecodeAccelerationSupportTest, Callback);

  ~VideoDecodeAccelerationSupport();

  static void OnFrameReadyCallback(void* callback_data,
                                   CFDictionaryRef frame_info,
                                   OSStatus status,
                                   uint32_t flags,
                                   CVImageBufferRef image_buffer);
  void OnFrameReady(CVImageBufferRef image_buffer,
                    OSStatus status,
                    int frame_id);

  // Reference to the VDA decoder.
  VDADecoder decoder_;

  // A monotonically increasing number. This is used to assign a unique ID for
  // decode frame request.
  int frame_id_count_;

  // Maps a unique ID to the callback given to the decode function.
  std::map<int, FrameReadyCallback> frame_ready_callbacks_;

  // A reference to the message loop that this object was created on. This is
  // used to invoke the frame ready callback on the correct thread.
  scoped_refptr<base::MessageLoopProxy> loop_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAccelerationSupport);
};

}  // namespace gfx

#endif  // UI_GFX_VIDEO_DECODE_ACCELERATION_SUPPORT_MAC_H_
