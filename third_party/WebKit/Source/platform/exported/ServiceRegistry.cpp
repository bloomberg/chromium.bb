// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/InterfaceProvider.h"

#include "platform/wtf/StdLibExtras.h"

namespace blink {
namespace {

class EmptyInterfaceProvider : public InterfaceProvider {
  void GetInterface(const char* name, mojo::ScopedMessagePipeHandle) override {}
};
}

InterfaceProvider* InterfaceProvider::GetEmptyInterfaceProvider() {
  DEFINE_STATIC_LOCAL(EmptyInterfaceProvider, empty_interface_provider, ());
  return &empty_interface_provider;
}
}
