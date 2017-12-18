// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_provider_win.h"

#include <windows.media.faceanalysis.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace shape_detection {

namespace {

using ABI::Windows::Media::FaceAnalysis::IFaceDetectorStatics;
using base::win::ScopedHString;
using base::win::GetActivationFactory;

BitmapPixelFormat GetPreferredPixelFormat(IFaceDetectorStatics* factory) {
  static constexpr BitmapPixelFormat kFormats[] = {
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Gray8,
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Nv12};

  for (const auto& format : kFormats) {
    boolean is_supported = false;
    factory->IsBitmapPixelFormatSupported(format, &is_supported);
    if (is_supported)
      return format;
  }
  return ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Unknown;
}

}  // namespace

void FaceDetectionProviderWin::CreateFaceDetection(
    shape_detection::mojom::FaceDetectionRequest request,
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  if (async_create_detector_ops_) {
    mojo::ReportBadMessage(
        "FaceDetectionProvider client may only create one FaceDetection at a "
        "time.");
    return;
  }

  // FaceDetector class is only available in Win 10 onwards (v10.0.10240.0).
  if (base::win::GetVersion() < base::win::VERSION_WIN10) {
    DVLOG(1) << "FaceDetector not supported before Windows 10";
    return;
  }
  // Loads functions dynamically at runtime to prevent library dependencies.
  if (!(base::win::ResolveCoreWinRTDelayload() &&
        ScopedHString::ResolveCoreWinRTStringDelayload())) {
    DLOG(ERROR) << "Failed loading functions from combase.dll";
    return;
  }

  Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory;
  HRESULT hr = GetActivationFactory<
      IFaceDetectorStatics,
      RuntimeClass_Windows_Media_FaceAnalysis_FaceDetector>(&factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IFaceDetectorStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  boolean is_supported = false;
  factory->get_IsSupported(&is_supported);
  if (!is_supported)
    return;

  // In the current version, the FaceDetector class only supports images in
  // Gray8 or Nv12. Gray8 should be a good type but verify it against
  // FaceDetector’s supported formats.
  BitmapPixelFormat pixel_format = GetPreferredPixelFormat(factory.Get());
  if (pixel_format ==
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Unknown) {
    return;
  }

  // Create an instance of FaceDetector asynchronously.
  AsyncOperation<FaceDetector>::IAsyncOperationPtr async_op;
  hr = factory->CreateAsync(&async_op);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create FaceDetector failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  // The once callback will not be called if this object is deleted, so it's
  // fine to use Unretained to bind the callback.
  auto async_operation = AsyncOperation<FaceDetector>::Create(
      base::BindOnce(&FaceDetectionProviderWin::OnFaceDetectorCreated,
                     base::Unretained(this), std::move(request), pixel_format),
      std::move(async_op));
  if (!async_operation)
    return;

  async_create_detector_ops_ = std::move(async_operation);
  // When |provider| goes out of scope it will immediately closes its end of
  // the message pipe, then |async_create_detector_ops_| will be deleted and the
  // callback OnFaceDetectorCreated will be not called. This prevents this
  // object from being destroyed before the AsyncOperation completes.
  binding_->PauseIncomingMethodCallProcessing();
}

FaceDetectionProviderWin::FaceDetectionProviderWin() = default;
FaceDetectionProviderWin::~FaceDetectionProviderWin() = default;

void FaceDetectionProviderWin::OnFaceDetectorCreated(
    shape_detection::mojom::FaceDetectionRequest request,
    BitmapPixelFormat pixel_format,
    AsyncOperation<FaceDetector>::IAsyncOperationPtr async_op) {
  binding_->ResumeIncomingMethodCallProcessing();
  async_create_detector_ops_.reset();

  Microsoft::WRL::ComPtr<IFaceDetector> face_detector;
  HRESULT hr =
      async_op ? async_op->GetResults(face_detector.GetAddressOf()) : E_FAIL;
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetResults failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  Microsoft::WRL::ComPtr<ISoftwareBitmapStatics> bitmap_factory;
  hr = GetActivationFactory<
      ISoftwareBitmapStatics,
      RuntimeClass_Windows_Graphics_Imaging_SoftwareBitmap>(&bitmap_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "ISoftwareBitmapStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  auto impl = std::make_unique<FaceDetectionImplWin>(
      std::move(face_detector), std::move(bitmap_factory), pixel_format);
  auto* impl_ptr = impl.get();
  impl_ptr->SetBinding(
      mojo::MakeStrongBinding(std::move(impl), std::move(request)));
}

}  // namespace shape_detection
