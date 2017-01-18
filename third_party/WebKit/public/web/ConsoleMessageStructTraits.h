// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessageStructTraits_h
#define ConsoleMessageStructTraits_h

#include "WebConsoleMessage.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/public/web/console_message.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<::blink::mojom::ConsoleMessageLevel,
                  ::blink::WebConsoleMessage::Level> {
  static ::blink::mojom::ConsoleMessageLevel ToMojom(
      ::blink::WebConsoleMessage::Level);
  static bool FromMojom(::blink::mojom::ConsoleMessageLevel,
                        ::blink::WebConsoleMessage::Level* out);
};

}  // namespace mojo

#endif
