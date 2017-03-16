// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchRegistration.h"

#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/IconDefinition.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

BackgroundFetchRegistration::BackgroundFetchRegistration(
    ServiceWorkerRegistration* registration,
    String tag,
    HeapVector<IconDefinition> icons,
    long long totalDownloadSize,
    String title)
    : m_registration(registration),
      m_tag(tag),
      m_icons(icons),
      m_totalDownloadSize(totalDownloadSize),
      m_title(title) {}

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

String BackgroundFetchRegistration::tag() const {
  return m_tag;
}

HeapVector<IconDefinition> BackgroundFetchRegistration::icons() const {
  return m_icons;
}

long long BackgroundFetchRegistration::totalDownloadSize() const {
  return m_totalDownloadSize;
}

String BackgroundFetchRegistration::title() const {
  return m_title;
}

void BackgroundFetchRegistration::abort() {
  BackgroundFetchBridge::from(m_registration)->abort(m_tag);
}

DEFINE_TRACE(BackgroundFetchRegistration) {
  visitor->trace(m_registration);
  visitor->trace(m_icons);
}

}  // namespace blink
