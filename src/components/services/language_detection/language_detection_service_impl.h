// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LANGUAGE_DETECTION_LANGUAGE_DETECTION_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_LANGUAGE_DETECTION_LANGUAGE_DETECTION_SERVICE_IMPL_H_

#include "base/strings/string16.h"
#include "components/services/language_detection/public/mojom/language_detection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace language_detection {

// Language Detection Service implementation.
//
// This service implementation analyzes text content to determine the most
// likely language for it.
// It is intended to operate in an out-of-browser-process service.
class LanguageDetectionServiceImpl : public mojom::LanguageDetectionService {
 public:
  explicit LanguageDetectionServiceImpl(
      mojo::PendingReceiver<mojom::LanguageDetectionService> receiver);
  ~LanguageDetectionServiceImpl() override;

 private:
  // chrome::mojom::LanguageDetectionService override.
  void DetermineLanguage(const ::base::string16& text,
                         DetermineLanguageCallback callback) override;

  mojo::Receiver<mojom::LanguageDetectionService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(LanguageDetectionServiceImpl);
};

}  // namespace language_detection

#endif  // COMPONENTS_SERVICES_LANGUAGE_DETECTION_LANGUAGE_DETECTION_SERVICE_IMPL_H_
