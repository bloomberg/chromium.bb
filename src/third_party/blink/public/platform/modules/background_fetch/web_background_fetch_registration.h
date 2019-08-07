// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_BACKGROUND_FETCH_WEB_BACKGROUND_FETCH_REGISTRATION_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_BACKGROUND_FETCH_WEB_BACKGROUND_FETCH_REGISTRATION_H_

#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

// Represents a BackgroundFetchRegistration object, added mainly for layering.
// Analogous to the following structure in the spec:
// https://wicg.github.io/background-fetch/#background-fetch-registration
// Also contains a message pipe for registration functionality.
struct WebBackgroundFetchRegistration {
  WebBackgroundFetchRegistration(
      const WebString& developer_id,
      uint64_t upload_total,
      uint64_t uploaded,
      uint64_t download_total,
      uint64_t downloaded,
      mojom::BackgroundFetchResult result,
      mojom::BackgroundFetchFailureReason failure_reason,
      mojo::ScopedMessagePipeHandle registration_service_handle,
      uint32_t registration_service_version)
      : developer_id(developer_id),
        upload_total(upload_total),
        uploaded(uploaded),
        download_total(download_total),
        downloaded(downloaded),
        result(result),
        failure_reason(failure_reason),
        registration_service_handle(std::move(registration_service_handle)),
        registration_service_version(registration_service_version) {}

  ~WebBackgroundFetchRegistration() = default;
  WebBackgroundFetchRegistration(WebBackgroundFetchRegistration&&) = default;

  WebString developer_id;
  uint64_t upload_total;
  uint64_t uploaded;
  uint64_t download_total;
  uint64_t downloaded;
  mojom::BackgroundFetchResult result;
  mojom::BackgroundFetchFailureReason failure_reason;

  // This should be connected to the BackgroundFetchRegistrationService
  // interface.
  mojo::ScopedMessagePipeHandle registration_service_handle;
  uint32_t registration_service_version;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_BACKGROUND_FETCH_WEB_BACKGROUND_FETCH_REGISTRATION_H_
