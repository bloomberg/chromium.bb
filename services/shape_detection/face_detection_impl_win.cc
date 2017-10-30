// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_win.h"

#include <windows.media.faceanalysis.h>

#include "base/scoped_generic.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "media/base/scoped_callback_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/face_detection_provider_impl.h"

namespace shape_detection {

using base::win::ScopedHString;
using base::win::GetActivationFactory;

void FaceDetectionProviderImpl::CreateFaceDetection(
    shape_detection::mojom::FaceDetectionRequest request,
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  auto impl = FaceDetectionImplWin::Create();
  if (!impl)
    return;

  mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

// static
std::unique_ptr<FaceDetectionImplWin> FaceDetectionImplWin::Create() {
  // FaceDetector class is only available in Win 10 onwards (v10.0.10240.0).
  if (base::win::GetVersion() < base::win::VERSION_WIN10) {
    DVLOG(1) << "FaceDetector not supported before Windows 10";
    return nullptr;
  }
  // Loads functions dynamically at runtime to prevent library dependencies.
  if (!(base::win::ResolveCoreWinRTDelayload() &&
        ScopedHString::ResolveCoreWinRTStringDelayload())) {
    DLOG(ERROR) << "Failed loading functions from combase.dll";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory;
  const HRESULT hr = GetActivationFactory<
      IFaceDetectorStatics,
      RuntimeClass_Windows_Media_FaceAnalysis_FaceDetector>(&factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IFaceDetectorStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  boolean is_supported = FALSE;
  factory->get_IsSupported(&is_supported);
  return (is_supported == FALSE)
             ? nullptr
             : std::make_unique<FaceDetectionImplWin>(std::move(factory));
}

FaceDetectionImplWin::FaceDetectionImplWin(
    Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory)
    : face_detector_factory_(std::move(factory)) {}

FaceDetectionImplWin::~FaceDetectionImplWin() = default;

void FaceDetectionImplWin::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  DCHECK_EQ(kN32_SkColorType, bitmap.colorType());
}

}  // namespace shape_detection