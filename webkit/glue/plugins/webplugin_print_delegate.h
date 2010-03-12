// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_WEBPLUGIN_PRINT_DELEGATE_H_
#define WEBKIT_GLUE_PLUGINS_WEBPLUGIN_PRINT_DELEGATE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace gfx {
class Rect;
}

namespace webkit_glue {

// Interface for the NPAPI print extension. This class implements "NOP"
// versions of all these functions so it can be used seamlessly by the
// "regular" plugin delegate while being overridden by the "pepper" one.
class WebPluginPrintDelegate {
 public:
  // If a plugin supports print extensions, then it gets to participate fully
  // in the browser's print workflow by specifying the number of pages to be
  // printed and providing a print output for specified pages.
  virtual bool PrintSupportsPrintExtension() {
    return false;
  }

  // Note: printable_area is in points (a point is 1/72 of an inch).
  virtual int PrintBegin(const gfx::Rect& printable_area, int printer_dpi) {
    return 0;
  }

  virtual bool PrintPage(int page_number, WebKit::WebCanvas* canvas) {
    return false;
  }

  virtual void PrintEnd() {
  }

 protected:
  WebPluginPrintDelegate() {}
  virtual ~WebPluginPrintDelegate() {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_PLUGINS_WEBPLUGIN_PRINT_DELEGATE_H_

