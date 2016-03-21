// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT ClipboardUtil {
 public:
  // Returns an NSPasteboardItem that represents the given |url|.
  // |url| must not be nil.
  // If |title| is nil, |url| is used in its place.
  static base::scoped_nsobject<NSPasteboardItem> PasteboardItemFromUrl(
      NSString* url,
      NSString* title);
};
}

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_MAC_H_
