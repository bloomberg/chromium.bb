// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_EXTENSION_H_
#define CONTENT_RENDERER_WEB_UI_EXTENSION_H_

#include <string>

#include "base/macros.h"

namespace blink {
class WebLocalFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class WebUIExtension {
 public:
  static void Install(blink::WebLocalFrame* frame);

 private:
  static void Send(gin::Arguments* args);
  static std::string GetVariableValue(const std::string& name);

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebUIExtension);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_EXTENSION_H_
