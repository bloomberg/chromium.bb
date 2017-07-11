// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_fuchsia.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

const char kMimeTypeBitmap[] = "image/bmp";

// Clipboard::FormatType implementation.
Clipboard::FormatType::FormatType() {}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {}

Clipboard::FormatType::~FormatType() {}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  return data_ < other.data_;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

// Various predefined FormatTypes.
// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeURIList));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  return GetUrlFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeText));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeRTF));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeBitmap));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
  return type;
}

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardFuchsia;
}

// ClipboardFuchsia implementation.

ClipboardFuchsia::ClipboardFuchsia() {
  DCHECK(CalledOnValidThread());
}

ClipboardFuchsia::~ClipboardFuchsia() {
  DCHECK(CalledOnValidThread());
}

void ClipboardFuchsia::OnPreShutdown() {}

uint64_t ClipboardFuchsia::GetSequenceNumber(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

bool ClipboardFuchsia::IsFormatAvailable(const Clipboard::FormatType& format,
                                         ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTIMPLEMENTED();
  return false;
}

void ClipboardFuchsia::Clear(ClipboardType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::ReadAvailableTypes(ClipboardType type,
                                          std::vector<base::string16>* types,
                                          bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTIMPLEMENTED();
  types->clear();
  *contains_filenames = false;
}

void ClipboardFuchsia::ReadText(ClipboardType type,
                                base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTREACHED();
}

void ClipboardFuchsia::ReadAsciiText(ClipboardType type,
                                     std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTREACHED();
}

// Note: |src_url| isn't really used. It is only implemented in Windows
void ClipboardFuchsia::ReadHTML(ClipboardType type,
                                base::string16* markup,
                                std::string* src_url,
                                uint32_t* fragment_start,
                                uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTREACHED();
}

void ClipboardFuchsia::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTREACHED();
}

SkBitmap ClipboardFuchsia::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTREACHED();
  return SkBitmap();
}

void ClipboardFuchsia::ReadCustomData(ClipboardType clipboard_type,
                                      const base::string16& type,
                                      base::string16* result) const {
  DCHECK(CalledOnValidThread());
  NOTREACHED();
}

void ClipboardFuchsia::ReadBookmark(base::string16* title,
                                    std::string* url) const {
  DCHECK(CalledOnValidThread());
  NOTREACHED();
}

void ClipboardFuchsia::ReadData(const Clipboard::FormatType& format,
                                std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  result->clear();
}

base::Time ClipboardFuchsia::GetLastModifiedTime() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return base::Time();
}

void ClipboardFuchsia::ClearLastModifiedTime() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// Main entry point used to write several values in the clipboard.
void ClipboardFuchsia::WriteObjects(ClipboardType type,
                                    const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteText(const char* text_data, size_t text_len) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteHTML(const char* markup_data,
                                 size_t markup_len,
                                 const char* url_data,
                                 size_t url_len) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteRTF(const char* rtf_data, size_t data_len) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteBookmark(const char* title_data,
                                     size_t title_len,
                                     const char* url_data,
                                     size_t url_len) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteWebSmartPaste() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteBitmap(const SkBitmap& bitmap) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardFuchsia::WriteData(const Clipboard::FormatType& format,
                                 const char* data_data,
                                 size_t data_len) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

}  // namespace ui
