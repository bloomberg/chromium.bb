// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_provider_win.h"

#include <windows.media.faceanalysis.h>

#include "base/scoped_generic.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "media/base/scoped_callback_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace shape_detection {

using ABI::Windows::Media::FaceAnalysis::FaceDetector;
using base::win::ScopedHString;
using base::win::GetActivationFactory;

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

  boolean is_supported = FALSE;
  factory->get_IsSupported(&is_supported);
  if (is_supported == FALSE)
    return;

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
                     base::Unretained(this), std::move(request),
                     std::move(factory)),
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
    Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory,
    AsyncOperation<FaceDetector>::IAsyncOperationPtr async_op) {
  binding_->ResumeIncomingMethodCallProcessing();
  async_create_detector_ops_.reset();

  Microsoft::WRL::ComPtr<IFaceDetector> face_detector;
  const HRESULT hr =
      async_op ? async_op->GetResults(face_detector.GetAddressOf()) : E_FAIL;
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetResults failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  mojo::MakeStrongBinding(base::MakeUnique<FaceDetectionImplWin>(
                              std::move(factory), std::move(face_detector)),
                          std::move(request));
}

}  // namespace shape_detection
