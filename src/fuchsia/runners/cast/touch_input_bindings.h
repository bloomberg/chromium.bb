// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_TOUCH_INPUT_BINDINGS_H_
#define FUCHSIA_RUNNERS_CAST_TOUCH_INPUT_BINDINGS_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/macros.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"

enum class TouchInputPolicy {
  UNSPECIFIED,
  FORCE_ENABLE,
  FORCE_DISABLE,
};

// Implements the native portions of the setTouchInputEnabled() Cast JS
// API.
class TouchInputBindings {
 public:
  // |policy|: Touch capabilities for the application declared by the
  //           ApplicationConfig service.
  // |frame|: The Frame which is hosting the Cast application.
  //          Must outlive |this|.
  // |connector|: Used to supply the messaging channel to the JS layer.
  TouchInputBindings(TouchInputPolicy policy,
                     fuchsia::web::Frame* frame,
                     NamedMessagePortConnector* connector);
  ~TouchInputBindings();

 private:
  // Receives a MessagePort from the API.
  void OnPortReceived(fidl::InterfaceHandle<fuchsia::web::MessagePort> port);

  // Processes setTouchInputEnabled() calls.
  void OnControlMessageReceived(fuchsia::web::WebMessage message);

  void ReadNextMessage();

  const TouchInputPolicy policy_;
  fuchsia::web::Frame* const frame_;
  NamedMessagePortConnector* const connector_;

  fuchsia::web::MessagePortPtr port_;

  DISALLOW_COPY_AND_ASSIGN(TouchInputBindings);
};

#endif  // FUCHSIA_RUNNERS_CAST_TOUCH_INPUT_BINDINGS_H_
