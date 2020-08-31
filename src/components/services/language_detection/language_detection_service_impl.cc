// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/language_detection/language_detection_service_impl.h"

#include <string>

#include "components/translate/core/language_detection/language_detection_util.h"

namespace language_detection {

LanguageDetectionServiceImpl::LanguageDetectionServiceImpl(
    mojo::PendingReceiver<mojom::LanguageDetectionService> receiver)
    : receiver_(this, std::move(receiver)) {}

LanguageDetectionServiceImpl::~LanguageDetectionServiceImpl() = default;

void LanguageDetectionServiceImpl::DetermineLanguage(
    const ::base::string16& text,
    DetermineLanguageCallback callback) {
  bool is_cld_reliable = false;
  std::string cld_language =
      translate::DetermineTextLanguage(text, &is_cld_reliable);
  std::move(callback).Run(cld_language, is_cld_reliable);
}

}  // namespace language_detection
