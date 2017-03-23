// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Connector_h
#define Connector_h

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"

namespace blink {

// Supports obtaining interfaces from services specified by name.
class BLINK_PLATFORM_EXPORT Connector {
 public:
  virtual void bindInterface(const char* serviceName,
                             const char* interfaceName,
                             mojo::ScopedMessagePipeHandle) = 0;

  template <typename Interface>
  void bindInterface(const char* serviceName,
                     mojo::InterfaceRequest<Interface> ptr) {
    bindInterface(serviceName, Interface::Name_, ptr.PassMessagePipe());
  }

  static Connector* getEmptyConnector();
};

}  // namespace blink

#endif
