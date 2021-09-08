// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_driver.h"

namespace translate {

TranslateDriver::TranslateDriver() = default;

TranslateDriver::~TranslateDriver() {
  for (auto& observer : language_detection_observers())
    observer.OnTranslateDriverDestroyed(this);
}

void TranslateDriver::AddLanguageDetectionObserver(
    LanguageDetectionObserver* observer) {
  language_detection_observers_.AddObserver(observer);
}

void TranslateDriver::RemoveLanguageDetectionObserver(
    LanguageDetectionObserver* observer) {
  language_detection_observers_.RemoveObserver(observer);
}

}  // namespace translate
