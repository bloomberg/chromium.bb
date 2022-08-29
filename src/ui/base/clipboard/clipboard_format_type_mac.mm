// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#import <Cocoa/Cocoa.h>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

// ClipboardFormatType implementation.
// MacOS formats are implemented via Uniform Type Identifiers, documented here:
// https://developer.apple.com/library/archive/documentation/General/Conceptual/DevPedia-CocoaCore/UniformTypeIdentifier.html#//apple_ref/doc/uid/TP40008195-CH60
ClipboardFormatType::ClipboardFormatType() : data_(nil) {}

ClipboardFormatType::ClipboardFormatType(NSString* native_format)
    : data_([native_format retain]) {}

ClipboardFormatType::ClipboardFormatType(const ClipboardFormatType& other)
    : data_([other.data_ retain]) {}

ClipboardFormatType& ClipboardFormatType::operator=(
    const ClipboardFormatType& other) {
  if (this != &other) {
    [data_ release];
    data_ = [other.data_ retain];
  }
  return *this;
}

bool ClipboardFormatType::operator==(const ClipboardFormatType& other) const {
  return [data_ isEqualToString:other.data_];
}

ClipboardFormatType::~ClipboardFormatType() {
  [data_ release];
}

std::string ClipboardFormatType::Serialize() const {
  return base::SysNSStringToUTF8(data_);
}

// static
ClipboardFormatType ClipboardFormatType::Deserialize(
    const std::string& serialization) {
  return ClipboardFormatType(base::SysUTF8ToNSString(serialization));
}

std::string ClipboardFormatType::GetName() const {
  return Serialize();
}

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return [data_ compare:other.data_] == NSOrderedAscending;
}

// static
std::string ClipboardFormatType::WebCustomFormatName(int index) {
  return base::StrCat({"com.web.custom.format", base::NumberToString(index)});
}

// static
std::string ClipboardFormatType::WebCustomFormatMapName() {
  return "com.web.custom.format.map";
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomFormatMap() {
  static base::NoDestructor<ClipboardFormatType> type(
      base::SysUTF8ToNSString(ClipboardFormatType::WebCustomFormatMapName()));
  return *type;
}

// static
ClipboardFormatType ClipboardFormatType::CustomPlatformType(
    const std::string& format_string) {
  DCHECK(base::IsStringASCII(format_string));
  return ClipboardFormatType::Deserialize(format_string);
}

// Various predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType::Deserialize(format_string);
}

// static
const ClipboardFormatType& ClipboardFormatType::FilenamesType() {
  static base::NoDestructor<ClipboardFormatType> type(NSFilenamesPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::UrlType() {
  static base::NoDestructor<ClipboardFormatType> type(NSURLPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PlainTextType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeString);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::HtmlType() {
  static base::NoDestructor<ClipboardFormatType> type(NSHTMLPboardType);
  return *type;
}

const ClipboardFormatType& ClipboardFormatType::SvgType() {
  static base::NoDestructor<ClipboardFormatType> type(kImageSvg);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::RtfType() {
  static base::NoDestructor<ClipboardFormatType> type(NSRTFPboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::PngType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypePNG);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::BitmapType() {
  static base::NoDestructor<ClipboardFormatType> type(NSPasteboardTypeTIFF);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebKitSmartPasteType() {
  static base::NoDestructor<ClipboardFormatType> type(kWebSmartPastePboardType);
  return *type;
}

// static
const ClipboardFormatType& ClipboardFormatType::WebCustomDataType() {
  static base::NoDestructor<ClipboardFormatType> type(kWebCustomDataPboardType);
  return *type;
}

}  // namespace ui
