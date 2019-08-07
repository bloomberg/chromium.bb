// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WEB_REQUEST_HOOKS_H_
#define EXTENSIONS_RENDERER_WEB_REQUEST_HOOKS_H_

#include <string>

#include "base/macros.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {

// Custom hooks for the WebRequest API.
class WebRequestHooks : public APIBindingHooksDelegate {
 public:
  WebRequestHooks();
  ~WebRequestHooks() override;

  // APIBindingHooksDelegate:
  // Creates a new WebRequest event.
  // TODO(devlin): WebRequest events are a very unfortunate implementation
  // detail, but refactoring means changing a few parts of the internal API.
  // It's not impossible, but it's a bit involved. However, as we move more
  // towards native bindings, it's definitely something we'll want to do.
  bool CreateCustomEvent(v8::Local<v8::Context> context,
                         const std::string& event_name,
                         v8::Local<v8::Value>* event_out) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestHooks);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WEB_REQUEST_HOOKS_H_
