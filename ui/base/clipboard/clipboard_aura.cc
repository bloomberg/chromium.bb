// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ui {

namespace {
const char kMimeTypeBitmap[] = "image/bmp";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
}

Clipboard::Clipboard() {
  NOTIMPLEMENTED();
}

Clipboard::~Clipboard() {
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  NOTIMPLEMENTED();
}


void Clipboard::WriteObjects(const ObjectMap& objects,
                             base::ProcessHandle process) {
  NOTIMPLEMENTED();
}

void Clipboard::DidWriteURL(const std::string& utf8_text) {
  NOTIMPLEMENTED();
}

bool Clipboard::IsFormatAvailable(const FormatType& format,
                                  Buffer buffer) const {
  NOTIMPLEMENTED();
  return false;
}

bool Clipboard::IsFormatAvailableByString(const std::string& format,
                                          Buffer buffer) const {
  NOTIMPLEMENTED();
  return false;
}

void Clipboard::ReadAvailableTypes(Buffer buffer, std::vector<string16>* types,
    bool* contains_filenames) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadText(Buffer buffer, string16* result) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadAsciiText(Buffer buffer, std::string* result) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadHTML(Buffer buffer, string16* markup,
    std::string* src_url) const {
  NOTIMPLEMENTED();
}

SkBitmap Clipboard::ReadImage(Buffer buffer) const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadFile(FilePath* file) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadFiles(std::vector<FilePath>* files) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadData(const std::string& format, std::string* result) {
  NOTIMPLEMENTED();
}

uint64 Clipboard::GetSequenceNumber() {
  NOTIMPLEMENTED();
  return 0;
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteWebSmartPaste() {
  NOTIMPLEMENTED();
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  NOTIMPLEMENTED();
}

void Clipboard::WriteData(const char* format_name, size_t format_len,
                          const char* data_data, size_t data_len) {
  NOTIMPLEMENTED();
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return std::string(kMimeTypeText);
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return std::string(kMimeTypeHTML);
}

// static
Clipboard::FormatType Clipboard::GetBitmapFormatType() {
  return std::string(kMimeTypeBitmap);
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  return std::string(kMimeTypeWebkitSmartPaste);
}

}  // namespace ui
