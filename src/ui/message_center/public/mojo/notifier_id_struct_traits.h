// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_MOJO_NOTIFIER_ID_STRUCT_TRAITS_H_
#define UI_MESSAGE_CENTER_PUBLIC_MOJO_NOTIFIER_ID_STRUCT_TRAITS_H_

#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/public/mojo/notifier_id.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<message_center::mojom::NotifierType,
                  message_center::NotifierType> {
  static message_center::mojom::NotifierType ToMojom(
      message_center::NotifierType input) {
    switch (input) {
      case message_center::NotifierType::APPLICATION:
        return message_center::mojom::NotifierType::APPLICATION;
      case message_center::NotifierType::ARC_APPLICATION:
        return message_center::mojom::NotifierType::ARC_APPLICATION;
      case message_center::NotifierType::WEB_PAGE:
        return message_center::mojom::NotifierType::WEB_PAGE;
      case message_center::NotifierType::SYSTEM_COMPONENT:
        return message_center::mojom::NotifierType::SYSTEM_COMPONENT;
      case message_center::NotifierType::CROSTINI_APPLICATION:
        return message_center::mojom::NotifierType::CROSTINI_APPLICATION;
    }
    NOTREACHED();
    return message_center::mojom::NotifierType::APPLICATION;
  }

  static bool FromMojom(message_center::mojom::NotifierType input,
                        message_center::NotifierType* out) {
    switch (input) {
      case message_center::mojom::NotifierType::APPLICATION:
        *out = message_center::NotifierType::APPLICATION;
        return true;
      case message_center::mojom::NotifierType::ARC_APPLICATION:
        *out = message_center::NotifierType::ARC_APPLICATION;
        return true;
      case message_center::mojom::NotifierType::WEB_PAGE:
        *out = message_center::NotifierType::WEB_PAGE;
        return true;
      case message_center::mojom::NotifierType::SYSTEM_COMPONENT:
        *out = message_center::NotifierType::SYSTEM_COMPONENT;
        return true;
      case message_center::mojom::NotifierType::CROSTINI_APPLICATION:
        *out = message_center::NotifierType::CROSTINI_APPLICATION;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<message_center::mojom::NotifierIdDataView,
                    message_center::NotifierId> {
  static message_center::NotifierType type(const message_center::NotifierId& n);
  static const std::string& id(const message_center::NotifierId& n);
  static const GURL& url(const message_center::NotifierId& n);
  static const std::string& profile_id(const message_center::NotifierId& n);
  static bool Read(message_center::mojom::NotifierIdDataView data,
                   message_center::NotifierId* out);
};

}  // namespace mojo

#endif  // UI_MESSAGE_CENTER_PUBLIC_MOJO_NOTIFIER_ID_STRUCT_TRAITS_H_
