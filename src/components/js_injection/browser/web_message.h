// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_H_
#define COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_H_

#include <string>
#include <vector>

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace js_injection {

// Represents a message to or from the page.
struct WebMessage {
  WebMessage();
  ~WebMessage();

  std::u16string message;
  std::vector<blink::MessagePortDescriptor> ports;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_H_
