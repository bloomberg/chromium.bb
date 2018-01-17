// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationStructTraits_h
#define NotificationStructTraits_h

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "platform/PlatformExport.h"
#include "platform/mojo/CommonCustomTypesStructTraits.h"
#include "platform/mojo/KURLStructTraits.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/notifications/WebNotificationAction.h"
#include "public/platform/modules/notifications/notification.mojom-blink.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT EnumTraits<blink::mojom::NotificationDirection,
                                  blink::WebNotificationData::Direction> {
  static blink::mojom::NotificationDirection ToMojom(
      blink::WebNotificationData::Direction input);

  static bool FromMojom(blink::mojom::NotificationDirection input,
                        blink::WebNotificationData::Direction* out);
};

template <>
struct PLATFORM_EXPORT EnumTraits<blink::mojom::NotificationActionType,
                                  blink::WebNotificationAction::Type> {
  static blink::mojom::NotificationActionType ToMojom(
      blink::WebNotificationAction::Type input);

  static bool FromMojom(blink::mojom::NotificationActionType input,
                        blink::WebNotificationAction::Type* out);
};

template <>
struct PLATFORM_EXPORT StructTraits<blink::mojom::NotificationActionDataView,
                                    blink::WebNotificationAction> {
  static blink::WebNotificationAction::Type type(
      const blink::WebNotificationAction& action) {
    return action.type;
  }

  static WTF::String action(const blink::WebNotificationAction&);

  static WTF::String title(const blink::WebNotificationAction&);

  static blink::KURL icon(const blink::WebNotificationAction&);

  static WTF::String placeholder(const blink::WebNotificationAction&);

  static bool Read(blink::mojom::NotificationActionDataView,
                   blink::WebNotificationAction* output);
};

template <>
struct PLATFORM_EXPORT StructTraits<blink::mojom::NotificationDataDataView,
                                    blink::WebNotificationData> {
  static WTF::String title(const blink::WebNotificationData&);

  static blink::WebNotificationData::Direction direction(
      const blink::WebNotificationData& data) {
    return data.direction;
  }

  static WTF::String lang(const blink::WebNotificationData&);

  static WTF::String body(const blink::WebNotificationData&);

  static WTF::String tag(const blink::WebNotificationData&);

  static blink::KURL image(const blink::WebNotificationData&);

  static blink::KURL icon(const blink::WebNotificationData&);

  static blink::KURL badge(const blink::WebNotificationData&);

  static base::span<const int32_t> vibration_pattern(
      const blink::WebNotificationData&);

  static double timestamp(const blink::WebNotificationData& data) {
    return data.timestamp;
  }

  static bool renotify(const blink::WebNotificationData& data) {
    return data.renotify;
  }

  static bool silent(const blink::WebNotificationData& data) {
    return data.silent;
  }

  static bool require_interaction(const blink::WebNotificationData& data) {
    return data.require_interaction;
  }

  static base::span<const int8_t> data(const blink::WebNotificationData&);

  static base::span<const blink::WebNotificationAction> actions(
      const blink::WebNotificationData&);

  static bool Read(blink::mojom::NotificationDataDataView,
                   blink::WebNotificationData* output);
};

}  // namespace mojo

#endif  // NotificationStructTraits_h
