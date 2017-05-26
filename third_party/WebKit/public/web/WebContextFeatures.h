// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebContextFeatures_h
#define WebContextFeatures_h

#include "public/platform/WebCommon.h"
#include "v8/include/v8.h"

namespace blink {

// WebContextFeatures is used in conjunction with IDL interface features which
// specify a [ContextEnabled] extended attribute. Such features may be enabled
// for arbitrary main-world V8 contexts by using these methods during
// WebFrameClient::DidCreateScriptContext. Enabling a given feature causes its
// binding(s) to be installed on the appropriate global object(s) during context
// initialization.
//
// See src/third_party/WebKit/Source/bindings/IDLExtendedAttributes.md for more
// information.
class WebContextFeatures {
 public:
  BLINK_EXPORT static void EnableMojoJS(v8::Local<v8::Context>, bool);

 private:
  WebContextFeatures();
};

}  // namespace blink

#endif  // WebContextFeatures_h
