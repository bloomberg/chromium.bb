// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_HOST_H_
#define WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_HOST_H_

#include <memory>

namespace weblayer {

struct WebMessage;

// Represents the browser side of a WebMessage channel. See
// WebMessageHostFactory for details.
class WebMessageHost {
 public:
  virtual ~WebMessageHost() = default;

  // Called when the page sends a message to the browser side.
  virtual void OnPostMessage(std::unique_ptr<WebMessage> message) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_HOST_H_
