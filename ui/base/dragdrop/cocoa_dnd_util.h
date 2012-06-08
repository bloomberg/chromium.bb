// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_MAC_DND_UTIL_H_
#define UI_BASE_DRAGDROP_MAC_DND_UTIL_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/string16.h"
#include "ui/base/ui_export.h"

class GURL;

namespace ui {

// Dummy data type that Chrome places in dragging pasteboards. There is never
// any data associated with this type; it's only used to ensure that Chrome
// supports any drag initiated inside of Chrome, whether or not data has been
// associated with it.
UI_EXPORT extern NSString* const kChromeDragDummyPboardType;

// Populates the |url| and |title| with URL data in |pboard|. There may be more
// than one, but we only handle dropping the first. |url| must not be |NULL|;
// |title| is an optional parameter. Returns |YES| if URL data was obtained from
// the pasteboard, |NO| otherwise. If |convert_filenames| is |YES|, the function
// will also attempt to convert filenames in |pboard| to file URLs.
UI_EXPORT BOOL PopulateURLAndTitleFromPasteboard(GURL* url,
                                                 string16* title,
                                                 NSPasteboard* pboard,
                                                 BOOL convert_filenames);

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_MAC_DND_UTIL_H_
