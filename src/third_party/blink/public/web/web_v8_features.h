// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_V8_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_V8_FEATURES_H_

#include "third_party/blink/public/platform/web_common.h"
#include "v8/include/v8.h"

namespace blink {

// WebV8Features is used in conjunction with IDL interface features which
// specify a [ContextEnabled] extended attribute. Such features may be enabled
// for arbitrary main-world V8 contexts by using these methods during
// WebLocalFrameClient::DidCreateScriptContext. Enabling a given feature causes
// its binding(s) to be installed on the appropriate global object(s) during
// context initialization.
//
// See src/third_party/blink/renderer/bindings/IDLExtendedAttributes.md for more
// information.
class WebV8Features {
 public:
  BLINK_EXPORT static void EnableMojoJS(v8::Local<v8::Context>, bool);

  // Enables SharedArrayBuffer for this process.
  BLINK_EXPORT static void EnableSharedArrayBuffer();

  // Enables web assembly threads for this process.
  BLINK_EXPORT static void EnableWasmThreads();

 private:
  WebV8Features() = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_V8_FEATURES_H_
