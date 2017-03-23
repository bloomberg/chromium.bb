// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/Connector.h"
#include "public/platform/InterfaceProvider.h"

#include "wtf/StdLibExtras.h"

namespace blink {
namespace {

class EmptyConnector : public Connector {
  void bindInterface(const char* serviceName,
                     const char* interfaceName,
                     mojo::ScopedMessagePipeHandle) override {}
};

class EmptyInterfaceProvider : public InterfaceProvider {
  void getInterface(const char* name, mojo::ScopedMessagePipeHandle) override {}
};
}

Connector* Connector::getEmptyConnector() {
  DEFINE_STATIC_LOCAL(EmptyConnector, emptyConnector, ());
  return &emptyConnector;
}

InterfaceProvider* InterfaceProvider::getEmptyInterfaceProvider() {
  DEFINE_STATIC_LOCAL(EmptyInterfaceProvider, emptyInterfaceProvider, ());
  return &emptyInterfaceProvider;
}
}
