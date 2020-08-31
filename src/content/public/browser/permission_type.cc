// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_type.h"

#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionStatus;

namespace content {

const std::vector<PermissionType>& GetAllPermissionTypes() {
  static const base::NoDestructor<std::vector<PermissionType>>
      kAllPermissionTypes([] {
        const int NUM_TYPES = static_cast<int>(PermissionType::NUM);
        std::vector<PermissionType> all_types;
        all_types.reserve(NUM_TYPES - 4);
        for (int i = 1; i < NUM_TYPES; ++i) {
          if (i == 2 || i == 14 || i == 15)  // Skip removed entries.
            continue;
          all_types.push_back(static_cast<PermissionType>(i));
        }
        return all_types;
      }());
  return *kAllPermissionTypes;
}

base::Optional<PermissionType> PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor) {
  switch (descriptor->name) {
    case PermissionName::GEOLOCATION:
      return PermissionType::GEOLOCATION;
    case PermissionName::NOTIFICATIONS:
      return PermissionType::NOTIFICATIONS;
    case PermissionName::MIDI: {
      if (descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex) {
        return PermissionType::MIDI_SYSEX;
      }
      return PermissionType::MIDI;
    }
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
#if defined(ENABLE_PROTECTED_MEDIA_IDENTIFIER_PERMISSION)
      return PermissionType::PROTECTED_MEDIA_IDENTIFIER;
#else
      NOTIMPLEMENTED();
      return base::nullopt;
#endif  // defined(ENABLE_PROTECTED_MEDIA_IDENTIFIER_PERMISSION)
    case PermissionName::DURABLE_STORAGE:
      return PermissionType::DURABLE_STORAGE;
    case PermissionName::AUDIO_CAPTURE:
      return PermissionType::AUDIO_CAPTURE;
    case PermissionName::VIDEO_CAPTURE:
      if (descriptor->extension && descriptor->extension->is_camera_device() &&
          descriptor->extension->get_camera_device()->panTiltZoom) {
        return PermissionType::CAMERA_PAN_TILT_ZOOM;
      } else {
        return PermissionType::VIDEO_CAPTURE;
      }
    case PermissionName::BACKGROUND_SYNC:
      return PermissionType::BACKGROUND_SYNC;
    case PermissionName::SENSORS:
      return PermissionType::SENSORS;
    case PermissionName::ACCESSIBILITY_EVENTS:
      return PermissionType::ACCESSIBILITY_EVENTS;
    case PermissionName::CLIPBOARD_READ:
      return PermissionType::CLIPBOARD_READ_WRITE;
    case PermissionName::CLIPBOARD_WRITE: {
      if (descriptor->extension && descriptor->extension->is_clipboard() &&
          descriptor->extension->get_clipboard()->allowWithoutSanitization) {
        return PermissionType::CLIPBOARD_READ_WRITE;
      } else {
        return PermissionType::CLIPBOARD_SANITIZED_WRITE;
      }
    }
    case PermissionName::PAYMENT_HANDLER:
      return PermissionType::PAYMENT_HANDLER;
    case PermissionName::BACKGROUND_FETCH:
      return PermissionType::BACKGROUND_FETCH;
    case PermissionName::IDLE_DETECTION:
      return PermissionType::IDLE_DETECTION;
    case PermissionName::PERIODIC_BACKGROUND_SYNC:
      return PermissionType::PERIODIC_BACKGROUND_SYNC;
    case PermissionName::SCREEN_WAKE_LOCK:
      return PermissionType::WAKE_LOCK_SCREEN;
    case PermissionName::SYSTEM_WAKE_LOCK:
      return PermissionType::WAKE_LOCK_SYSTEM;
    case PermissionName::NFC:
      return PermissionType::NFC;
    case PermissionName::STORAGE_ACCESS:
      return PermissionType::STORAGE_ACCESS_GRANT;
    case PermissionName::WINDOW_PLACEMENT:
      return PermissionType::WINDOW_PLACEMENT;
  }

  NOTREACHED();
  return base::nullopt;
}

}  // namespace content
