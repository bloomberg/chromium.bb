// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/shape_detection_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "services/shape_detection/text_detection_impl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "services/shape_detection/shape_detection_jni_headers/InterfaceRegistrar_jni.h"
#endif

#if defined(OS_MAC)
#include "services/shape_detection/barcode_detection_provider_mac.h"
#elif defined(OS_ANDROID)
// No C++ code, barcode detection comes from Java.
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "services/shape_detection/barcode_detection_provider_barhopper.h"
#else
#include "services/shape_detection/barcode_detection_provider_impl.h"
#endif

#if defined(OS_WIN)
#include "services/shape_detection/face_detection_provider_win.h"
#elif defined(OS_MAC)
#include "services/shape_detection/face_detection_provider_mac.h"
#elif defined(OS_ANDROID)
// No C++ code, face detection comes from Java.
#else
#include "services/shape_detection/face_detection_provider_impl.h"
#endif

namespace shape_detection {

ShapeDetectionService::ShapeDetectionService(
    mojo::PendingReceiver<mojom::ShapeDetectionService> receiver)
    : receiver_(this, std::move(receiver)) {
}

ShapeDetectionService::~ShapeDetectionService() = default;

void ShapeDetectionService::BindBarcodeDetectionProvider(
    mojo::PendingReceiver<mojom::BarcodeDetectionProvider> receiver) {
#if defined(OS_ANDROID)
  Java_InterfaceRegistrar_bindBarcodeDetectionProvider(
      base::android::AttachCurrentThread(),
      receiver.PassPipe().release().value());
#elif defined(OS_MAC)
  BarcodeDetectionProviderMac::Create(std::move(receiver));
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS))
  BarcodeDetectionProviderBarhopper::Create(std::move(receiver));
#else
  BarcodeDetectionProviderImpl::Create(std::move(receiver));
#endif
}

void ShapeDetectionService::BindFaceDetectionProvider(
    mojo::PendingReceiver<mojom::FaceDetectionProvider> receiver) {
#if defined(OS_ANDROID)
  Java_InterfaceRegistrar_bindFaceDetectionProvider(
      base::android::AttachCurrentThread(),
      receiver.PassPipe().release().value());
#elif defined(OS_MAC)
  FaceDetectionProviderMac::Create(std::move(receiver));
#elif defined(OS_WIN)
  FaceDetectionProviderWin::Create(std::move(receiver));
#else
  FaceDetectionProviderImpl::Create(std::move(receiver));
#endif
}

void ShapeDetectionService::BindTextDetection(
    mojo::PendingReceiver<mojom::TextDetection> receiver) {
#if defined(OS_ANDROID)
  Java_InterfaceRegistrar_bindTextDetection(
      base::android::AttachCurrentThread(),
      receiver.PassPipe().release().value());
#else
  TextDetectionImpl::Create(std::move(receiver));
#endif
}

}  // namespace shape_detection
