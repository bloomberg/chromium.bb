// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.#ifndef WebViewBase_h

#ifndef WebLocalFrameBase_h
#define WebLocalFrameBase_h

#include "core/CoreExport.h"
#include "public/web/WebLocalFrame.h"

namespace blink {

class LocalFrame;

// WebLocalFrameBase is a temporary class the provides a layer of abstraction
// for WebLocalFrameImpl. Mehtods that are declared public in WebLocalFrameImpl
// that are not overrides from WebLocalFrame will be declared pure virtual in
// WebLocalFrameBase. Classes that then have a dependency on WebLocalFrameImpl
// will then take a dependency on WebLocalFrameBase instead, so we can remove
// cyclic dependencies in web/ and move classes from web/ into core/ or
// modules.
// TODO(slangley): Remove this class once WebLocalFrameImpl is in core/.
class WebLocalFrameBase : public WebLocalFrame {
 public:
  CORE_EXPORT static WebLocalFrameBase* FromFrame(LocalFrame*);
  CORE_EXPORT static WebLocalFrameBase* FromFrame(LocalFrame&);

 protected:
  explicit WebLocalFrameBase(WebTreeScopeType scope) : WebLocalFrame(scope) {}
};
}

#endif  // WebLocalFrameBase_h
