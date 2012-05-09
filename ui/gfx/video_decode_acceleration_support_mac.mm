// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/video_decode_acceleration_support_mac.h"

#include <dlfcn.h>

#include "base/bind.h"
#include "base/file_path.h"
#import "base/mac/foundation_util.h"
#include "base/location.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/native_library.h"

using base::mac::CFToNSCast;
using base::mac::NSToCFCast;

const CFStringRef kVDADecoderConfiguration_Width = CFSTR("width");
const CFStringRef kVDADecoderConfiguration_Height = CFSTR("height");
const CFStringRef kVDADecoderConfiguration_SourceFormat = CFSTR("format");
const CFStringRef kVDADecoderConfiguration_avcCData = CFSTR("avcC");
const CFStringRef kCVPixelBufferIOSurfacePropertiesKey =
    CFSTR("IOSurfaceProperties");

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
enum {
  kVDADecoderNoErr = 0,
  kVDADecoderHardwareNotSupportedErr = -12470,
  kVDADecoderFormatNotSupportedErr = -12471,
  kVDADecoderFlush_EmitFrames = 1 << 0,
};

typedef void (*VDADecoderOutputCallback)(
    void* decompressionOutputRefCon,
    CFDictionaryRef frameInfo,
    OSStatus status,
    uint32_t infoFlags,
    CVImageBufferRef imageBuffer);
#endif  // MAC_OS_X_VERSION_10_6

namespace {

// These are 10.6.3 APIs that we look up at run time.
typedef OSStatus (*VDADecoderCreateFunc)(
    CFDictionaryRef decoderConfiguration,
    CFDictionaryRef destinationImageBufferAttributes,
    VDADecoderOutputCallback* outputCallback,
    void* decoderOutputCallbackRefcon,
    VDADecoder* decoderOut);

typedef OSStatus (*VDADecoderDecodeFunc)(
    VDADecoder decoder,
    uint32_t decodeFlags,
    CFTypeRef compressedBuffer,
    CFDictionaryRef frameInfo);

typedef OSStatus (*VDADecoderFlushFunc)(
    VDADecoder decoder,
    uint32_t flushFlags);

typedef OSStatus (*VDADecoderDestroyFunc)(
    VDADecoder decoder);

VDADecoderCreateFunc g_VDADecoderCreate;
VDADecoderDecodeFunc g_VDADecoderDecode;
VDADecoderFlushFunc g_VDADecoderFlush;
VDADecoderDestroyFunc g_VDADecoderDestroy;

NSString* const kFrameIdKey = @"frame_id";

bool InitializeVdaApis() {
  static bool should_initialize = true;
  if (should_initialize) {
    should_initialize = false;
    FilePath path;
    if (!base::mac::GetSearchPathDirectory(
            NSLibraryDirectory, NSSystemDomainMask, &path)) {
      return false;
    }

    path  = path.AppendASCII("Frameworks")
                .AppendASCII("VideoDecodeAcceleration.framework")
                .AppendASCII("VideoDecodeAcceleration");
    base::NativeLibrary vda_library = base::LoadNativeLibrary(path, NULL);
    if (!vda_library)
      return false;

    g_VDADecoderCreate = reinterpret_cast<VDADecoderCreateFunc>(
        base::GetFunctionPointerFromNativeLibrary(
            vda_library, "VDADecoderCreate"));
    g_VDADecoderDecode = reinterpret_cast<VDADecoderDecodeFunc>(
        base::GetFunctionPointerFromNativeLibrary(
            vda_library, "VDADecoderDecode"));
    g_VDADecoderFlush = reinterpret_cast<VDADecoderFlushFunc>(
        base::GetFunctionPointerFromNativeLibrary(
            vda_library, "VDADecoderFlush"));
    g_VDADecoderDestroy = reinterpret_cast<VDADecoderDestroyFunc>(
        base::GetFunctionPointerFromNativeLibrary(
            vda_library, "VDADecoderDestroy"));
  }

  return g_VDADecoderCreate && g_VDADecoderDecode && g_VDADecoderFlush &&
         g_VDADecoderDestroy;
}

}  // namespace

namespace gfx {

VideoDecodeAccelerationSupport::VideoDecodeAccelerationSupport()
    : decoder_(NULL),
      frame_id_count_(0),
      loop_(base::MessageLoopProxy::current()) {
}

VideoDecodeAccelerationSupport::Status VideoDecodeAccelerationSupport::Create(
    int width,
    int height,
    int pixel_format,
    const void* avc_bytes,
    size_t avc_size) {
  if (!InitializeVdaApis())
    return VDA_LOAD_FRAMEWORK_ERROR;

  NSData* avc_data = [NSData dataWithBytes:avc_bytes length:avc_size];
  NSDictionary* config = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithInt:width],
      CFToNSCast(kVDADecoderConfiguration_Width),
      [NSNumber numberWithInt:height],
      CFToNSCast(kVDADecoderConfiguration_Height),
      [NSNumber numberWithInt:'avc1'],
      CFToNSCast(kVDADecoderConfiguration_SourceFormat),
      avc_data,
      CFToNSCast(kVDADecoderConfiguration_avcCData),
      nil];

  NSDictionary* format_info = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithInt:pixel_format],
      CFToNSCast(kCVPixelBufferPixelFormatTypeKey),
      // Must set to an empty dictionary as per documentation.
      [NSDictionary dictionary],
      CFToNSCast(kCVPixelBufferIOSurfacePropertiesKey),
      nil];

  OSStatus status = g_VDADecoderCreate(NSToCFCast(config),
      NSToCFCast(format_info),
      reinterpret_cast<VDADecoderOutputCallback*>(OnFrameReadyCallback), this,
      &decoder_);
  switch (status) {
    case kVDADecoderNoErr:
      return VDA_SUCCESS;
    case kVDADecoderHardwareNotSupportedErr:
      return VDA_HARDWARE_NOT_SUPPORTED_ERROR;
    default:
      return VDA_OTHER_ERROR;
  }
}

VideoDecodeAccelerationSupport::Status VideoDecodeAccelerationSupport::Decode(
    const void* bytes,
    size_t size,
    const FrameReadyCallback& cb) {
  DCHECK(decoder_);
  ++frame_id_count_;
  frame_ready_callbacks_[frame_id_count_] = cb;

  NSData* data = [NSData dataWithBytes:bytes length:size];
  NSDictionary* frame_info = [NSDictionary
      dictionaryWithObject:[NSNumber numberWithInt:frame_id_count_]
                    forKey:kFrameIdKey];

  OSStatus status = g_VDADecoderDecode(decoder_, 0, NSToCFCast(data),
      NSToCFCast(frame_info));
  if (status != kVDADecoderNoErr) {
    frame_ready_callbacks_.erase(frame_id_count_);
    return VDA_OTHER_ERROR;
  }
  return VDA_SUCCESS;
}

VideoDecodeAccelerationSupport::Status VideoDecodeAccelerationSupport::Flush(
    bool emit_frames) {
  DCHECK(decoder_);
  OSStatus status = g_VDADecoderFlush(
      decoder_, emit_frames ? kVDADecoderFlush_EmitFrames : 0);
  if (!emit_frames)
    frame_ready_callbacks_.clear();
  return status == kVDADecoderNoErr ? VDA_SUCCESS : VDA_OTHER_ERROR;
}

VideoDecodeAccelerationSupport::Status
VideoDecodeAccelerationSupport::Destroy() {
  DCHECK(decoder_);
  OSStatus status = g_VDADecoderDestroy(decoder_);
  decoder_ = NULL;
  return status == kVDADecoderNoErr ? VDA_SUCCESS : VDA_OTHER_ERROR;
}

VideoDecodeAccelerationSupport::~VideoDecodeAccelerationSupport() {
  DCHECK(!decoder_);
}

// static
void VideoDecodeAccelerationSupport::OnFrameReadyCallback(
    void* callback_data,
    CFDictionaryRef frame_info,
    OSStatus status,
    uint32_t flags,
    CVImageBufferRef image_buffer) {
  base::mac::ScopedNSAutoreleasePool pool;
  NSNumber* frame_id_num = base::mac::ObjCCastStrict<NSNumber>(
      [CFToNSCast(frame_info) objectForKey:kFrameIdKey]);
  int frame_id = [frame_id_num intValue];

  // Retain the image while the callback is in flight.
  if (image_buffer)
    CFRetain(image_buffer);

  VideoDecodeAccelerationSupport* vda =
      reinterpret_cast<VideoDecodeAccelerationSupport*>(callback_data);
  DCHECK(vda);
  vda->loop_->PostTask(
      FROM_HERE, base::Bind(&VideoDecodeAccelerationSupport::OnFrameReady, vda,
      image_buffer, status, frame_id));
}

void VideoDecodeAccelerationSupport::OnFrameReady(CVImageBufferRef image_buffer,
                                                  OSStatus status,
                                                  int frame_id) {
  frame_ready_callbacks_[frame_id].Run(image_buffer, status);
  // Match the retain in OnFrameReadyCallback.
  if (image_buffer)
    CFRelease(image_buffer);
  frame_ready_callbacks_.erase(frame_id);
}

}  // namespace
