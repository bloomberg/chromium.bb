// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

// Various format we support.
const char kPlainTextFormat[] = "text";
const char kHTMLFormat[] = "html";
const char kRTFFormat[] = "rtf";
const char kBitmapFormat[] = "bitmap";
const char kWebKitSmartPasteFormat[] = "webkit_smart";
const char kBookmarkFormat[] = "bookmark";
const char kMimeTypeWebCustomData[] = "chromium/x-web-custom-data";

}  // namespace

Clipboard::FormatType::FormatType() {
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
}

Clipboard::FormatType::~FormatType() {
}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

Clipboard::Clipboard() : set_text_(NULL), has_text_(NULL), get_text_(NULL) {
}

Clipboard::~Clipboard() {
}

void Clipboard::WriteObjects(Buffer buffer, const ObjectMap& objects) {
}

uint64 Clipboard::GetSequenceNumber(Clipboard::Buffer /* buffer */) {
  return 0;
}

bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format,
                                  Clipboard::Buffer buffer) const {
  return false;
}

void Clipboard::Clear(Buffer buffer) {

}

void Clipboard::ReadAvailableTypes(Buffer buffer, std::vector<string16>* types,
                                   bool* contains_filenames) const {
}

void Clipboard::ReadText(Clipboard::Buffer buffer, string16* result) const {
}

void Clipboard::ReadAsciiText(Clipboard::Buffer buffer,
                              std::string* result) const {
}

void Clipboard::ReadHTML(Clipboard::Buffer buffer,
                         string16* markup,
                         std::string* src_url,
                         uint32* fragment_start,
                         uint32* fragment_end) const {
}

void Clipboard::ReadRTF(Buffer buffer, std::string* result) const {
}

SkBitmap Clipboard::ReadImage(Buffer buffer) const {
  return SkBitmap();
}

void Clipboard::ReadCustomData(Buffer buffer,
                               const string16& type,
                               string16* result) const {
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
}

void Clipboard::ReadData(const Clipboard::FormatType& format,
                         std::string* result) const {
}

// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPlainTextFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPlainTextFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kWebKitSmartPasteFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kHTMLFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kRTFFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kBitmapFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
}

void Clipboard::WriteRTF(const char* rtf_data, size_t data_len) {
}

void Clipboard::WriteBookmark(const char* title_data, size_t title_len,
                              const char* url_data, size_t url_len) {
}

void Clipboard::WriteWebSmartPaste() {
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
}

void Clipboard::WriteData(const Clipboard::FormatType& format,
                          const char* data_data, size_t data_len) {
}

bool Clipboard::IsTextAvailableFromAndroid() const {
  return false;
}

void Clipboard::ValidateInternalClipboard() const {
}

void Clipboard::Clear() {
}

void Clipboard::ClearInternalClipboard() const {
}

void Clipboard::Set(const std::string& key, const std::string& value) {
}

}  // namespace ui
