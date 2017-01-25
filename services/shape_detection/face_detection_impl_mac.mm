// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/shared_memory.h"
#include "media/capture/video/scoped_result_callback.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/shape_detection/face_detection_provider_impl.h"

namespace shape_detection {

namespace {

// kCIFormatRGBA8 is not exposed to public until Mac 10.11. So we define the
// same constant to support RGBA8 format in earlier versions.
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
const int kCIFormatRGBA8 = 24;
#endif

void RunCallbackWithFaces(
    const shape_detection::mojom::FaceDetection::DetectCallback& callback,
    shape_detection::mojom::FaceDetectionResultPtr faces) {
  callback.Run(std::move(faces));
}

void RunCallbackWithNoFaces(
    const shape_detection::mojom::FaceDetection::DetectCallback& callback) {
  callback.Run(shape_detection::mojom::FaceDetectionResult::New());
}

}  // anonymous namespace

void FaceDetectionProviderImpl::CreateFaceDetection(
    shape_detection::mojom::FaceDetectionRequest request,
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  mojo::MakeStrongBinding(
      base::MakeUnique<FaceDetectionImplMac>(std::move(options)),
      std::move(request));
}

FaceDetectionImplMac::FaceDetectionImplMac(
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  context_.reset([[CIContext alloc] init]);
  NSDictionary* const opts = @{CIDetectorAccuracy : CIDetectorAccuracyHigh};
  detector_.reset([[CIDetector detectorOfType:CIDetectorTypeFace
                                      context:context_
                                      options:opts] retain]);
}

FaceDetectionImplMac::~FaceDetectionImplMac() {}

void FaceDetectionImplMac::Detect(mojo::ScopedSharedBufferHandle frame_data,
                                  uint32_t width,
                                  uint32_t height,
                                  const DetectCallback& callback) {
  media::ScopedResultCallback<DetectCallback> scoped_callback(
      base::Bind(&RunCallbackWithFaces, callback),
      base::Bind(&RunCallbackWithNoFaces));

  base::CheckedNumeric<uint32_t> num_pixels =
      base::CheckedNumeric<uint32_t>(width) * height;
  base::CheckedNumeric<uint32_t> num_bytes = num_pixels * 4;
  if (!num_bytes.IsValid()) {
    DLOG(ERROR) << "Data overflow";
    return;
  }

  base::SharedMemoryHandle memory_handle;
  size_t memory_size = 0;
  bool read_only_flag = false;
  const MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(frame_data), &memory_handle, &memory_size, &read_only_flag);
  DCHECK_EQ(MOJO_RESULT_OK, result) << "Failed to unwrap SharedBufferHandle";
  if (!memory_size || memory_size != num_bytes.ValueOrDie()) {
    DLOG(ERROR) << "Invalid image size";
    return;
  }

  auto shared_memory =
      base::MakeUnique<base::SharedMemory>(memory_handle, true /* read_only */);
  if (!shared_memory->Map(memory_size)) {
    DLOG(ERROR) << "Failed to map bytes from shared memory";
    return;
  }

  NSData* byte_data = [NSData dataWithBytesNoCopy:shared_memory->memory()
                                           length:num_bytes.ValueOrDie()
                                     freeWhenDone:NO];

  base::ScopedCFTypeRef<CGColorSpaceRef> colorspace(
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB));

  // CIImage will return nil when RGBA8 is not supported in a certain version.
  base::scoped_nsobject<CIImage> ci_image([[CIImage alloc]
      initWithBitmapData:byte_data
             bytesPerRow:width * 4
                    size:CGSizeMake(width, height)
                  format:kCIFormatRGBA8
              colorSpace:colorspace]);
  if (!ci_image) {
    DLOG(ERROR) << "Failed to create CIImage";
    return;
  }

  NSArray* const features = [detector_ featuresInImage:ci_image];

  shape_detection::mojom::FaceDetectionResultPtr faces =
      shape_detection::mojom::FaceDetectionResult::New();
  for (CIFaceFeature* const f in features) {
    // In the default Core Graphics coordinate space, the origin is located
    // in the lower-left corner, and thus |ci_image| is flipped vertically.
    // We need to adjust |y| coordinate of bounding box before sending it.
    gfx::RectF boundingbox(f.bounds.origin.x,
                           height - f.bounds.origin.y - f.bounds.size.height,
                           f.bounds.size.width, f.bounds.size.height);
    faces->bounding_boxes.push_back(boundingbox);
  }
  scoped_callback.Run(std::move(faces));
}

}  // namespace shape_detection
