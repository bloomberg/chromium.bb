// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_win.h"

#include <windows.media.faceanalysis.h>

#include "base/bind.h"
#include "base/logging.h"

namespace shape_detection {

namespace {

using ABI::Windows::Media::FaceAnalysis::DetectedFace;

}  // namespace

FaceDetectionImplWin::FaceDetectionImplWin(
    Microsoft::WRL::ComPtr<IFaceDetector> face_detector,
    Microsoft::WRL::ComPtr<ISoftwareBitmapStatics> bitmap_factory,
    BitmapPixelFormat pixel_format)
    : face_detector_(std::move(face_detector)),
      bitmap_factory_(std::move(bitmap_factory)),
      pixel_format_(pixel_format) {
  DCHECK(face_detector_);
  DCHECK(bitmap_factory_);
}
FaceDetectionImplWin::~FaceDetectionImplWin() = default;

void FaceDetectionImplWin::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  if ((async_detect_face_ops_ = BeginDetect(bitmap))) {
    // Hold on the callback until AsyncOperation completes.
    detected_face_callback_ = std::move(callback);
    // This prevents the Detect function from being called before the
    // AsyncOperation completes.
    binding_->PauseIncomingMethodCallProcessing();
  } else {
    // No detection taking place; run |callback| with an empty array of results.
    std::move(callback).Run(std::vector<mojom::FaceDetectionResultPtr>());
  }
}

std::unique_ptr<AsyncOperation<IVector<DetectedFace*>>>
FaceDetectionImplWin::BeginDetect(const SkBitmap& bitmap) {
  Microsoft::WRL::ComPtr<ISoftwareBitmap> win_bitmap =
      CreateWinBitmapWithPixelFormat(bitmap, bitmap_factory_.Get(),
                                     pixel_format_);
  if (!win_bitmap)
    return nullptr;

  // Detect faces asynchronously.
  AsyncOperation<IVector<DetectedFace*>>::IAsyncOperationPtr async_op;
  const HRESULT hr =
      face_detector_->DetectFacesAsync(win_bitmap.Get(), &async_op);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Detect faces asynchronously failed: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  // The once callback will not be called if this object is deleted, so it's
  // fine to use Unretained to bind the callback. |win_bitmap| needs to be kept
  // alive until OnFaceDetected().
  return AsyncOperation<IVector<DetectedFace*>>::Create(
      base::BindOnce(&FaceDetectionImplWin::OnFaceDetected,
                     base::Unretained(this), std::move(win_bitmap)),
      std::move(async_op));
}

std::vector<mojom::FaceDetectionResultPtr>
FaceDetectionImplWin::BuildFaceDetectionResult(
    AsyncOperation<IVector<DetectedFace*>>::IAsyncOperationPtr async_op) {
  std::vector<mojom::FaceDetectionResultPtr> results;
  Microsoft::WRL::ComPtr<IVector<DetectedFace*>> detected_face;
  HRESULT hr =
      async_op ? async_op->GetResults(detected_face.GetAddressOf()) : E_FAIL;
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetResults failed: "
                << logging::SystemErrorCodeToString(hr);
    return results;
  }

  uint32_t count;
  hr = detected_face->get_Size(&count);
  if (FAILED(hr)) {
    DLOG(ERROR) << "get_Size failed: " << logging::SystemErrorCodeToString(hr);
    return results;
  }

  results.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    Microsoft::WRL::ComPtr<IDetectedFace> face;
    hr = detected_face->GetAt(i, &face);
    if (FAILED(hr))
      break;

    ABI::Windows::Graphics::Imaging::BitmapBounds bounds;
    hr = face->get_FaceBox(&bounds);
    if (FAILED(hr))
      break;

    auto result = shape_detection::mojom::FaceDetectionResult::New();
    result->bounding_box =
        gfx::RectF(bounds.X, bounds.Y, bounds.Width, bounds.Height);
    results.push_back(std::move(result));
  }
  return results;
}

// |win_bitmap| is passed here so that it is kept alive until the AsyncOperation
// completes because DetectFacesAsync does not hold a reference.
void FaceDetectionImplWin::OnFaceDetected(
    Microsoft::WRL::ComPtr<ISoftwareBitmap> /* win_bitmap */,
    AsyncOperation<IVector<DetectedFace*>>::IAsyncOperationPtr async_op) {
  std::move(detected_face_callback_)
      .Run(BuildFaceDetectionResult(std::move(async_op)));
  async_detect_face_ops_.reset();
  binding_->ResumeIncomingMethodCallProcessing();
}

}  // namespace shape_detection
