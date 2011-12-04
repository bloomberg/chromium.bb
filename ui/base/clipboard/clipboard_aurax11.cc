// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/linux_util.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {
const char kMimeTypeBitmap[] = "image/bmp";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardData {
 public:
  ClipboardData()
      : bitmap_data_(),
        custom_data_data_(),
        custom_data_len_(0),
        web_smart_paste_(false) {}

  virtual ~ClipboardData() {}

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; }

  const std::string& markup_data() const { return markup_data_; }
  void set_markup_data(const std::string& markup_data) {
    markup_data_ = markup_data;
  }

  const std::string& url() const { return url_; }
  void set_url(const std::string& url) { url_ = url; }

  const std::string& bookmark_title() const { return bookmark_title_; }
  void set_bookmark_title(const std::string& bookmark_title) {
    bookmark_title_ = bookmark_title;
  }

  const std::string& bookmark_url() const { return bookmark_url_; }
  void set_bookmark_url(const std::string& bookmark_url) {
    bookmark_url_ = bookmark_url;
  }

  uint8_t* bitmap_data() const { return bitmap_data_.get(); }
  const gfx::Size& bitmap_size() const { return bitmap_size_; }
  void SetBitmapData(const char* pixel_data, const char* size_data) {
    bitmap_size_ = *reinterpret_cast<const gfx::Size*>(size_data);

    // We assume 4-byte pixel data.
    size_t bitmap_data_len = 4 * bitmap_size_.width() * bitmap_size_.height();
    bitmap_data_.reset(new uint8_t[bitmap_data_len]);
    memcpy(bitmap_data_.get(), pixel_data, bitmap_data_len);
  }

  const std::string& custom_data_format() const { return custom_data_format_; }
  char* custom_data_data() const { return custom_data_data_.get(); }
  size_t custom_data_len() const { return custom_data_len_; }

  void SetCustomData(const std::string& data_format,
                     const char* data_data,
                     size_t data_len) {
    custom_data_len_ = data_len;
    if (custom_data_len_ == 0) {
      custom_data_data_.reset();
      custom_data_format_.clear();
      return;
    }
    custom_data_data_.reset(new char[custom_data_len_]);
    memcpy(custom_data_data_.get(), data_data, custom_data_len_);
    custom_data_format_ = data_format;
  }

  bool web_smart_paste() const { return web_smart_paste_; }
  void set_web_smart_paste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
  }

 private:
  // Plain text in UTF8 format.
  std::string text_;

  // HTML markup data in UTF8 format.
  std::string markup_data_;
  std::string url_;

  // Bookmark title in UTF8 format.
  std::string bookmark_title_;
  std::string bookmark_url_;

  // Bitmap images.
  scoped_array<uint8_t> bitmap_data_;
  gfx::Size bitmap_size_;

  // Data with custom format.
  std::string custom_data_format_;
  scoped_array<char> custom_data_data_;
  size_t custom_data_len_;

  // WebKit smart paste data.
  bool web_smart_paste_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

ClipboardData* clipboard_data = NULL;

ClipboardData* GetClipboardData() {
  if (!clipboard_data)
    clipboard_data = new ClipboardData();
  return clipboard_data;
}

void DeleteClipboardData() {
  if (clipboard_data)
    delete clipboard_data;
  clipboard_data = NULL;
}

}  // namespace

// TODO(varunjain): Complete implementation:
// 1. Handle different types of BUFFERs.
// 2. Do we need to care about concurrency here? Can there be multiple instances
//    of ui::Clipboard? Ask oshima.
// 3. Implement File types.
// 4. Handle conversion between types.

Clipboard::Clipboard() {
  // Make sure clipboard is created.
  GetClipboardData();
}

Clipboard::~Clipboard() {
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  // We need to overwrite previous data. Probably best to just delete
  // everything and start fresh.
  DeleteClipboardData();
  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
}

bool Clipboard::IsFormatAvailable(const FormatType& format,
                                  Buffer buffer) const {
  ClipboardData* data = GetClipboardData();
  if (GetPlainTextFormatType() == format)
    return !data->text().empty();
  else if (GetHtmlFormatType() == format)
    return !data->markup_data().empty() || !data->url().empty();
  else if (GetBitmapFormatType() == format)
    return !!data->bitmap_data();
  else if (GetWebKitSmartPasteFormatType() == format)
    return data->web_smart_paste();
  else if (data->custom_data_format() == format)
    return true;
  return false;
}

bool Clipboard::IsFormatAvailableByString(const std::string& format,
                                          Buffer buffer) const {
  return IsFormatAvailable(format, buffer);
}

void Clipboard::ReadAvailableTypes(Buffer buffer, std::vector<string16>* types,
    bool* contains_filenames) const {
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  types->clear();
  if (IsFormatAvailable(GetPlainTextFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetPlainTextFormatType()));
  if (IsFormatAvailable(GetHtmlFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetHtmlFormatType()));
  if (IsFormatAvailable(GetBitmapFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetBitmapFormatType()));
  if (IsFormatAvailable(GetWebKitSmartPasteFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetWebKitSmartPasteFormatType()));
  *contains_filenames = false;
}

void Clipboard::ReadText(Buffer buffer, string16* result) const {
  *result = UTF8ToUTF16(GetClipboardData()->text());
}

void Clipboard::ReadAsciiText(Buffer buffer, std::string* result) const {
  *result = GetClipboardData()->text();
}

void Clipboard::ReadHTML(Buffer buffer,
                         string16* markup,
                         std::string* src_url,
                         uint32* fragment_start,
                         uint32* fragment_end) const {
  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  *markup = UTF8ToUTF16(GetClipboardData()->markup_data());
  *src_url = GetClipboardData()->url();

  *fragment_start = 0;
  DCHECK(markup->length() <= kuint32max);
  *fragment_end = static_cast<uint32>(markup->length());

}

SkBitmap Clipboard::ReadImage(Buffer buffer) const {
  const gfx::Size size = GetClipboardData()->bitmap_size();
  uint8_t* bitmap = GetClipboardData()->bitmap_data();
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height(), 0);
  image.allocPixels();
  image.eraseARGB(0, 0, 0, 0);
  int byte_counter = 0;
  for (int i = 0; i < size.height(); ++i) {
    for (int j = 0; j < size.width(); ++j) {
      uint32* pixel = image.getAddr32(j, i);
      *pixel = (bitmap[byte_counter] << 0) +      /* R */
               (bitmap[byte_counter + 1] << 8) +  /* G */
               (bitmap[byte_counter + 2] << 16) + /* B */
               (bitmap[byte_counter + 3] << 24);  /* A */
      byte_counter += 4;
    }
  }
  return image;
}

void Clipboard::ReadCustomData(Buffer buffer,
                               const string16& type,
                               string16* result) const {
  // TODO(dcheng): Implement this.
  NOTIMPLEMENTED();
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  *title = UTF8ToUTF16(GetClipboardData()->bookmark_title());
  *url = GetClipboardData()->bookmark_url();
}

void Clipboard::ReadFile(FilePath* file) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadFiles(std::vector<FilePath>* files) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadData(const std::string& format, std::string* result) {
  result->clear();
  ClipboardData* data = GetClipboardData();
  if (data->custom_data_format() == format)
    *result = std::string(data->custom_data_data(), data->custom_data_len());
}

uint64 Clipboard::GetSequenceNumber(Buffer buffer) {
  NOTIMPLEMENTED();
  return 0;
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  GetClipboardData()->set_text(std::string(text_data, text_len));
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  GetClipboardData()->set_markup_data(std::string(markup_data, markup_len));
  GetClipboardData()->set_url(std::string(url_data, url_len));
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  GetClipboardData()->set_bookmark_title(std::string(title_data, title_len));
  GetClipboardData()->set_bookmark_url(std::string(url_data, url_len));
}

void Clipboard::WriteWebSmartPaste() {
  GetClipboardData()->set_web_smart_paste(true);
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  GetClipboardData()->SetBitmapData(pixel_data, size_data);
}

void Clipboard::WriteData(const char* format_name, size_t format_len,
                          const char* data_data, size_t data_len) {
  GetClipboardData()->SetCustomData(std::string(format_name, format_len),
      data_data, data_len);
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
