// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

// Would be nice if this were in UTCoreTypes.h, but it isn't
NSString* const kUTTypeURLName = @"public.url-name";

// Tells us if WebKit was the last to write to the pasteboard. There's no
// actual data associated with this type.
NSString* const kWebSmartPastePboardType = @"NeXT smart paste pasteboard type";

NSPasteboard* GetPasteboard() {
  // The pasteboard should not be nil in a UI session, but this handy DCHECK
  // can help track down problems if someone tries using clipboard code outside
  // of a UI session.
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  DCHECK(pasteboard);
  return pasteboard;
}

}  // namespace

Clipboard::Clipboard() {
}

Clipboard::~Clipboard() {
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:[NSArray array] owner:nil];

  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }

}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  std::string text_str(text_data, text_len);
  NSString *text = base::SysUTF8ToNSString(text_str);
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pb setString:text forType:NSStringPboardType];
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  // We need to mark it as utf-8. (see crbug.com/11957)
  std::string html_fragment_str("<meta charset='utf-8'>");
  html_fragment_str.append(markup_data, markup_len);
  NSString *html_fragment = base::SysUTF8ToNSString(html_fragment_str);

  // TODO(avi): url_data?
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSHTMLPboardType] owner:nil];
  [pb setString:html_fragment forType:NSHTMLPboardType];
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  std::string title_str(title_data, title_len);
  NSString *title =  base::SysUTF8ToNSString(title_str);
  std::string url_str(url_data, url_len);
  NSString *url =  base::SysUTF8ToNSString(url_str);

  // TODO(playmobil): In the Windows version of this function, an HTML
  // representation of the bookmark is also added to the clipboard, to support
  // drag and drop of web shortcuts.  I don't think we need to do this on the
  // Mac, but we should double check later on.
  NSURL* nsurl = [NSURL URLWithString:url];

  NSPasteboard* pb = GetPasteboard();
  // passing UTIs into the pasteboard methods is valid >= 10.5
  [pb addTypes:[NSArray arrayWithObjects:NSURLPboardType,
                                         kUTTypeURLName,
                                         nil]
         owner:nil];
  [nsurl writeToPasteboard:pb];
  [pb setString:title forType:kUTTypeURLName];
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  const gfx::Size* size = reinterpret_cast<const gfx::Size*>(size_data);

  // Safe because the image goes away before the call returns.
  base::mac::ScopedCFTypeRef<CFDataRef> data(
      CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                  reinterpret_cast<const UInt8*>(pixel_data),
                                  size->width()*size->height()*4,
                                  kCFAllocatorNull));

  base::mac::ScopedCFTypeRef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithCFData(data));

  base::mac::ScopedCFTypeRef<CGImageRef> cgimage(
      CGImageCreate(size->width(),
                    size->height(),
                    8,
                    32,
                    size->width()*4,
                    base::mac::GetSRGBColorSpace(),  // TODO(avi): do better
                    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                    data_provider,
                    NULL,
                    false,
                    kCGRenderingIntentDefault));
  // Aggressively free storage since image buffers can potentially be very
  // large.
  data_provider.reset();
  data.reset();

  scoped_nsobject<NSBitmapImageRep> bitmap(
      [[NSBitmapImageRep alloc] initWithCGImage:cgimage]);
  cgimage.reset();

  scoped_nsobject<NSImage> image([[NSImage alloc] init]);
  [image addRepresentation:bitmap];

  // An API to ask the NSImage to write itself to the clipboard comes in 10.6 :(
  // For now, spit out the image as a TIFF.
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
  NSData *tiff_data = [image TIFFRepresentation];
  LOG_IF(ERROR, tiff_data == NULL) << "Failed to allocate image for clipboard";
  if (tiff_data) {
    [pb setData:tiff_data forType:NSTIFFPboardType];
  }
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  NSPasteboard* pb = GetPasteboard();
  NSString* format = base::SysUTF8ToNSString(GetWebKitSmartPasteFormatType());
  [pb addTypes:[NSArray arrayWithObject:format] owner:nil];
  [pb setData:nil forType:format];
}

bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format,
                                  Clipboard::Buffer buffer) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  NSString* format_ns = base::SysUTF8ToNSString(format);

  NSPasteboard* pb = GetPasteboard();
  NSArray* types = [pb types];

  // Safari only places RTF on the pasteboard, never HTML. We can convert RTF
  // to HTML, so the presence of either indicates success when looking for HTML.
  if ([format_ns isEqualToString:NSHTMLPboardType]) {
    return [types containsObject:NSHTMLPboardType] ||
           [types containsObject:NSRTFPboardType];
  }
  return [types containsObject:format_ns];
}

void Clipboard::ReadText(Clipboard::Buffer buffer, string16* result) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSStringPboardType];

  UTF8ToUTF16([contents UTF8String],
              [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
              result);
}

void Clipboard::ReadAsciiText(Clipboard::Buffer buffer,
                              std::string* result) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSStringPboardType];

  if (!contents)
    result->clear();
  else
    result->assign([contents UTF8String]);
}

void Clipboard::ReadHTML(Clipboard::Buffer buffer, string16* markup,
                         std::string* src_url) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (markup) {
    NSPasteboard* pb = GetPasteboard();
    NSArray* supportedTypes = [NSArray arrayWithObjects:NSHTMLPboardType,
                                                        NSRTFPboardType,
                                                        NSStringPboardType,
                                                        nil];
    NSString* bestType = [pb availableTypeFromArray:supportedTypes];
    NSString* contents = [pb stringForType:bestType];
    if ([bestType isEqualToString:NSRTFPboardType])
      contents = [pb htmlFromRtf];
    UTF8ToUTF16([contents UTF8String],
                [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
                markup);
  }

  // TODO(avi): src_url?
  if (src_url)
    src_url->clear();
}

void Clipboard::ReadImage(Buffer buffer, std::string* data) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (!data) {
    NOTREACHED();
    return;
  }
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  NSPasteboard* pb = GetPasteboard();

  if (title) {
    NSString* contents = [pb stringForType:kUTTypeURLName];
    UTF8ToUTF16([contents UTF8String],
                [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
                title);
  }

  if (url) {
    NSString* url_string = [[NSURL URLFromPasteboard:pb] absoluteString];
    if (!url_string)
      url->clear();
    else
      url->assign([url_string UTF8String]);
  }
}

void Clipboard::ReadFile(FilePath* file) const {
  if (!file) {
    NOTREACHED();
    return;
  }

  *file = FilePath();
  std::vector<FilePath> files;
  ReadFiles(&files);

  // Take the first file, if available.
  if (!files.empty())
    *file = files[0];
}

void Clipboard::ReadFiles(std::vector<FilePath>* files) const {
  if (!files) {
    NOTREACHED();
    return;
  }

  files->clear();

  NSPasteboard* pb = GetPasteboard();
  NSArray* fileList = [pb propertyListForType:NSFilenamesPboardType];

  for (unsigned int i = 0; i < [fileList count]; ++i) {
    std::string file = [[fileList objectAtIndex:i] UTF8String];
    files->push_back(FilePath(file));
  }
}

// static
Clipboard::FormatType Clipboard::GetUrlFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSURLPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetUrlWFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSURLPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSStringPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSStringPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetFilenameFormatType() {
  static const std::string type =
      base::SysNSStringToUTF8(NSFilenamesPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetFilenameWFormatType() {
  static const std::string type =
      base::SysNSStringToUTF8(NSFilenamesPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSHTMLPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetBitmapFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSTIFFPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  static const std::string type =
      base::SysNSStringToUTF8(kWebSmartPastePboardType);
  return type;
}

}  // namespace ui
