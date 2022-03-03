// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_source_observer.h"

namespace ui {

ColorProviderSource::ColorProviderSource() = default;

ColorProviderSource::~ColorProviderSource() {
  for (auto& observer : observers_)
    observer.OnColorProviderSourceDestroying();
}

void ColorProviderSource::AddObserver(ColorProviderSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void ColorProviderSource::RemoveObserver(
    ColorProviderSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ColorProviderSource::NotifyColorProviderChanged() {
  for (auto& observer : observers_)
    observer.OnColorProviderChanged();
}

}  // namespace ui
