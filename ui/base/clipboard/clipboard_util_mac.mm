// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"

namespace {
NSString* const kWebURLsWithTitlesPboardType = @"WebURLsWithTitlesPboardType";
NSString* const kCorePasteboardFlavorType_url =
    @"CorePasteboardFlavorType 0x75726C20";  // 'url '  url
NSString* const kCorePasteboardFlavorType_urln =
    @"CorePasteboardFlavorType 0x75726C6E";  // 'urln'  title

// It's much more convenient to return an NSString than a
// base::ScopedCFTypeRef<CFStringRef>, since the methods on NSPasteboardItem
// require an NSString*.
NSString* UTIFromPboardType(NSString* type) {
  return [base::mac::CFToNSCast(UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassNSPboardType, base::mac::NSToCFCast(type), kUTTypeData))
      autorelease];
}
}  // namespace

namespace ui {

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromUrl(
    NSString* urlString,
    NSString* title) {
  DCHECK(urlString);
  if (!title)
    title = urlString;

  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);

  NSURL* url = [NSURL URLWithString:urlString];
  if ([url isFileURL] &&
      [[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
    [item setPropertyList:@[ [url path] ]
                  forType:UTIFromPboardType(NSFilenamesPboardType)];
  }

  // Set Safari's URL + title arrays Pboard type.
  NSArray* urlsAndTitles = @[ @[ urlString ], @[ title ] ];
  [item setPropertyList:urlsAndTitles
                forType:UTIFromPboardType(kWebURLsWithTitlesPboardType)];

  // Set NSURLPboardType. The format of the property list is divined from
  // Webkit's function PlatformPasteboard::setStringForType.
  // https://github.com/WebKit/webkit/blob/master/Source/WebCore/platform/mac/PlatformPasteboardMac.mm
  NSURL* base = [url baseURL];
  if (base) {
    [item setPropertyList:@[ [url relativeString], [base absoluteString] ]
                  forType:UTIFromPboardType(NSURLPboardType)];
  } else if (url) {
    [item setPropertyList:@[ [url absoluteString], @"" ]
                  forType:UTIFromPboardType(NSURLPboardType)];
  }

  [item setString:urlString forType:UTIFromPboardType(NSStringPboardType)];

  [item setData:[urlString dataUsingEncoding:NSUTF8StringEncoding]
        forType:UTIFromPboardType(kCorePasteboardFlavorType_url)];

  [item setData:[title dataUsingEncoding:NSUTF8StringEncoding]
        forType:UTIFromPboardType(kCorePasteboardFlavorType_urln)];
  return item;
}

}  // namespace ui
