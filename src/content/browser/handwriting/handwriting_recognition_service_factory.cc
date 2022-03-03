// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_factory.h"

#include <utility>

#include "build/buildflag.h"

#if defined(OS_CHROMEOS)
#include "content/browser/handwriting/handwriting_recognition_service_impl_cros.h"
#else
// The default service which does not have any real handwriting recognition
// backend.
#include "content/browser/handwriting/handwriting_recognition_service_impl.h"
#endif  // defined(OS_CHROMEOS)

namespace content {

void CreateHandwritingRecognitionService(
    mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>
        pending_receiver) {
#if defined(OS_CHROMEOS)
  CrOSHandwritingRecognitionServiceImpl::Create(std::move(pending_receiver));
#else
  HandwritingRecognitionServiceImpl::Create(std::move(pending_receiver));
#endif  // defined(OS_CHROMEOS)
}

}  // namespace content
