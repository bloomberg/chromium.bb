// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/webplugin_print_delegate.h"

namespace webkit_glue {

bool WebPluginPrintDelegate::PrintSupportsPrintExtension() {
  return false;
}

int WebPluginPrintDelegate::PrintBegin(const gfx::Rect& printable_area,
                                       int printer_dpi) {
  return 0;
}

bool WebPluginPrintDelegate::PrintPage(int page_number,
                                       WebKit::WebCanvas* canvas) {
  return false;
}

void WebPluginPrintDelegate::PrintEnd() {
}

}  // namespace webkit_glue
