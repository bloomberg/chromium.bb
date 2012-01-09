// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <list>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/linux_util.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {
const char kMimeTypeFilename[] = "chromium/filename";
const char kMimeTypeBitmap[] = "image/bmp";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
const size_t kMaxClipboardSize = 1;

// Clipboard data format used by AuraClipboard.
enum AuraClipboardFormat {
  TEXT      = 1 << 0,
  HTML      = 1 << 1,
  BOOKMARK  = 1 << 2,
  FILES     = 1 << 3,
  BITMAP    = 1 << 4,
  CUSTOM    = 1 << 5,
  WEB       = 1 << 6,
};

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardData {
 public:
  ClipboardData()
      : bitmap_data_(),
        custom_data_data_(),
        custom_data_len_(0),
        web_smart_paste_(false),
        format_(0) {}

  virtual ~ClipboardData() {}

  // Bitmask of AuraClipboardFormat types.
  const int format() const { return format_; }

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) {
    text_ = text;
    format_ |= TEXT;
  }

  const std::string& markup_data() const { return markup_data_; }
  void set_markup_data(const std::string& markup_data) {
    markup_data_ = markup_data;
    format_ |= HTML;
  }

  const std::string& url() const { return url_; }
  void set_url(const std::string& url) {
    url_ = url;
    format_ |= HTML;
  }

  const std::string& bookmark_title() const { return bookmark_title_; }
  void set_bookmark_title(const std::string& bookmark_title) {
    bookmark_title_ = bookmark_title;
    format_ |= BOOKMARK;
  }

  const std::string& bookmark_url() const { return bookmark_url_; }
  void set_bookmark_url(const std::string& bookmark_url) {
    bookmark_url_ = bookmark_url;
    format_ |= BOOKMARK;
  }

  const std::vector<std::string>& files() const { return files_; }
  void AddFile(const std::string& file) {
    files_.push_back(file);
    format_ |= FILES;
  }

  uint8_t* bitmap_data() const { return bitmap_data_.get(); }
  const gfx::Size& bitmap_size() const { return bitmap_size_; }
  void SetBitmapData(const char* pixel_data, const char* size_data) {
    bitmap_size_ = *reinterpret_cast<const gfx::Size*>(size_data);

    // We assume 4-byte pixel data.
    size_t bitmap_data_len = 4 * bitmap_size_.width() * bitmap_size_.height();
    bitmap_data_.reset(new uint8_t[bitmap_data_len]);
    memcpy(bitmap_data_.get(), pixel_data, bitmap_data_len);
    format_ |= BITMAP;
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
    format_ |= CUSTOM;
  }

  bool web_smart_paste() const { return web_smart_paste_; }
  void set_web_smart_paste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
    format_ |= WEB;
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

  // Filenames.
  std::vector<std::string> files_;

  // Bitmap images.
  scoped_array<uint8_t> bitmap_data_;
  gfx::Size bitmap_size_;

  // Data with custom format.
  std::string custom_data_format_;
  scoped_array<char> custom_data_data_;
  size_t custom_data_len_;

  // WebKit smart paste data.
  bool web_smart_paste_;

  int format_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

// Platform clipboard implementation for Aura. This handles things like format
// conversion, versioning of clipboard items etc. The goal is to roughly provide
// a substitute to platform clipboards on other platforms such as GtkClipboard
// on gtk or winapi clipboard on win.
class AuraClipboard {
 public:
  AuraClipboard() {}

  ~AuraClipboard() {
    STLDeleteContainerPointers(data_list_.begin(), data_list_.end());
    data_list_.clear();
  }

  // Returns the number of entries currently in the clipboard stack.
  size_t GetNumClipboardEntries() {
    return data_list_.size();
  }

  // Returns the data currently on the top of the clipboard stack, NULL if the
  // clipboard stack is empty.
  const ClipboardData* GetData() const {
    if (data_list_.empty())
      return NULL;
    return data_list_.front();
  }

  // Returns true if the data on top of the clipboard stack has format |format|
  // or another format that can be converted to |format|.
  bool IsFormatAvailable(AuraClipboardFormat format) const {
    switch (format) {
      case TEXT:
        return HasFormat(TEXT) || HasFormat(BOOKMARK) || HasFormat(FILES);
      default:
        return HasFormat(format);
    }
  }

  // Reads text from the data at the top of clipboard stack.
  void ReadText(string16* result) const {
    std::string utf8_result;
    ReadAsciiText(&utf8_result);
    *result = UTF8ToUTF16(utf8_result);
  }

  // Reads ascii text from the data at the top of clipboard stack.
  void ReadAsciiText(std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!data)
      return;
    if (HasFormat(TEXT))
      *result = data->text();
    else if (HasFormat(HTML))
      *result = data->markup_data();
    else if (HasFormat(BOOKMARK))
      *result = data->bookmark_url();
    else if (HasFormat(FILES)) {
      DCHECK_GT(data->files().size(), (size_t) 0);
      *result = data->files()[0];
    }
  }

  // Reads HTML from the data at the top of clipboard stack.
  void ReadHTML(string16* markup,
                std::string* src_url,
                uint32* fragment_start,
                uint32* fragment_end) const {
    markup->clear();
    if (src_url)
      src_url->clear();
    *fragment_start = 0;
    *fragment_end = 0;

    if (!HasFormat(HTML))
      return;

    const ClipboardData* data = GetData();
    *markup = UTF8ToUTF16(data->markup_data());
    *src_url = data->url();

    *fragment_start = 0;
    DCHECK_LE(markup->length(), kuint32max);
    *fragment_end = static_cast<uint32>(markup->length());
  }

  // Reads image from the data at the top of clipboard stack.
  SkBitmap ReadImage() const {
    SkBitmap img;
    if (!HasFormat(BITMAP))
      return img;

    const ClipboardData* data = GetData();
    const gfx::Size size = data->bitmap_size();
    uint8_t* bitmap = data->bitmap_data();
    img.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height(), 0);
    img.allocPixels();
    img.eraseARGB(0, 0, 0, 0);
    memcpy(img.getPixels(), bitmap, size.width() * size.height() * 4);
    return img;
  }

  // Reads data of type |type| from the data at the top of clipboard stack.
  void ReadCustomData(const string16& type, string16* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(CUSTOM) ||
        type != UTF8ToUTF16(data->custom_data_format()))
      return;

    *result = UTF8ToUTF16(std::string(data->custom_data_data(),
        data->custom_data_len()));
  }

  // Reads bookmark from the data at the top of clipboard stack.
  void ReadBookmark(string16* title, std::string* url) const {
    title->clear();
    url->clear();
    if (!HasFormat(BOOKMARK))
      return;

    const ClipboardData* data = GetData();
    *title = UTF8ToUTF16(data->bookmark_title());
    *url = data->bookmark_url();
  }

  // Reads a filename from the data at the top of clipboard stack.
  void ReadFile(FilePath* file) const {
    file->clear();
    if (!HasFormat(FILES))
      return;

    const ClipboardData* data = GetData();
    file->FromUTF8Unsafe(data->files()[0]);
  }

  void ReadFiles(std::vector<FilePath>* files) const {
    files->clear();
    if (!HasFormat(FILES))
      return;

    const ClipboardData* data = GetData();
    for (size_t i = 0; i < data->files().size(); ++i)
      files->push_back(FilePath(data->files()[i]));
  }

  void ReadData(const std::string& type, std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(CUSTOM) ||
        type != data->custom_data_format())
      return;

    *result = std::string(data->custom_data_data(), data->custom_data_len());
  }

  // Writes |data| to the top of the clipboard stack.
  void WriteData(ClipboardData* data) {
    DCHECK(data);
    AddToListEnsuringSize(data);
  }

 private:
  // True if the data on top of the clipboard stack has format |format|.
  bool HasFormat(AuraClipboardFormat format) const {
    const ClipboardData* data = GetData();
    if (!data)
      return false;

    return data->format() & format;
  }

  void AddToListEnsuringSize(ClipboardData* data) {
    DCHECK(data);
    data_list_.push_front(data);

    // If the size of list becomes more than the maximum allowed, we delete the
    // last element.
    if (data_list_.size() > kMaxClipboardSize) {
      ClipboardData* last = data_list_.back();
      data_list_.pop_back();
      delete last;
    }
  }

  // Stack containing various versions of ClipboardData.
  std::list<ClipboardData*> data_list_;

  DISALLOW_COPY_AND_ASSIGN(AuraClipboard);
};

AuraClipboard* aura_clipboard = NULL;

AuraClipboard* GetClipboard() {
  if (!aura_clipboard)
    aura_clipboard = new AuraClipboard();
  return aura_clipboard;
}

void DeleteClipboard() {
  if (aura_clipboard)
    delete aura_clipboard;
  aura_clipboard = NULL;
}

// Helper class to build a ClipboardData object and write it to clipboard.
class ClipboardDataBuilder {
 public:
  static void CommitToClipboard() {
    DCHECK(current_data_);
    GetClipboard()->WriteData(current_data_);
    current_data_ = NULL;
  }

  static void WriteText(const char* text_data, size_t text_len) {
    ClipboardData* data = GetCurrentData();
    data->set_text(std::string(text_data, text_len));
  }

  static void WriteHTML(const char* markup_data,
                        size_t markup_len,
                        const char* url_data,
                        size_t url_len) {
    ClipboardData* data = GetCurrentData();
    data->set_markup_data(std::string(markup_data, markup_len));
    data->set_url(std::string(url_data, url_len));
  }

  static void WriteBookmark(const char* title_data,
                            size_t title_len,
                            const char* url_data,
                            size_t url_len) {
    ClipboardData* data = GetCurrentData();
    data->set_bookmark_title(std::string(title_data, title_len));
    data->set_bookmark_url(std::string(url_data, url_len));
  }

  static void WriteWebSmartPaste() {
    ClipboardData* data = GetCurrentData();
    data->set_web_smart_paste(true);
  }

  static void WriteBitmap(const char* pixel_data, const char* size_data) {
    ClipboardData* data = GetCurrentData();
    data->SetBitmapData(pixel_data, size_data);
  }

  static void WriteData(const std::string& format,
                        const char* data_data,
                        size_t data_len) {
    ClipboardData* data = GetCurrentData();
    data->SetCustomData(format, data_data, data_len);
  }

 private:
  static ClipboardData* GetCurrentData() {
    if (!current_data_)
      current_data_ = new ClipboardData;
    return current_data_;
  }

  static ClipboardData* current_data_;
};

ClipboardData* ClipboardDataBuilder::current_data_ = NULL;

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

Clipboard::Clipboard() {
  // Make sure clipboard is created.
  GetClipboard();
}

Clipboard::~Clipboard() {
  DeleteClipboard();
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
  ClipboardDataBuilder::CommitToClipboard();
}

bool Clipboard::IsFormatAvailable(const FormatType& format,
                                  Buffer buffer) const {
  DCHECK(IsValidBuffer(buffer));
  AuraClipboard* clipboard = GetClipboard();
  if (GetPlainTextFormatType().Equals(format))
    return clipboard->IsFormatAvailable(TEXT);
  else if (GetHtmlFormatType().Equals(format))
    return clipboard->IsFormatAvailable(HTML);
  else if (GetBitmapFormatType().Equals(format))
    return clipboard->IsFormatAvailable(BITMAP);
  else if (GetFilenameFormatType().Equals(format))
    return clipboard->IsFormatAvailable(FILES);
  else if (GetWebKitSmartPasteFormatType().Equals(format))
    return clipboard->IsFormatAvailable(WEB);
  else {
    const ClipboardData* data = clipboard->GetData();
    if (data && data->custom_data_format() == format.ToString())
      return true;
  }
  return false;
}

void Clipboard::ReadAvailableTypes(Buffer buffer, std::vector<string16>* types,
    bool* contains_filenames) const {
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  types->clear();
  *contains_filenames = false;
  if (IsFormatAvailable(GetPlainTextFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetPlainTextFormatType().ToString()));
  if (IsFormatAvailable(GetHtmlFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetHtmlFormatType().ToString()));
  if (IsFormatAvailable(GetFilenameFormatType(), buffer)) {
    types->push_back(UTF8ToUTF16(GetFilenameFormatType().ToString()));
    *contains_filenames = true;
  }
  if (IsFormatAvailable(GetBitmapFormatType(), buffer))
    types->push_back(UTF8ToUTF16(GetBitmapFormatType().ToString()));
}

void Clipboard::ReadText(Buffer buffer, string16* result) const {
  GetClipboard()->ReadText(result);
}

void Clipboard::ReadAsciiText(Buffer buffer, std::string* result) const {
  GetClipboard()->ReadAsciiText(result);
}

void Clipboard::ReadHTML(Buffer buffer,
                         string16* markup,
                         std::string* src_url,
                         uint32* fragment_start,
                         uint32* fragment_end) const {
  GetClipboard()->ReadHTML(markup, src_url, fragment_start, fragment_end);
}

SkBitmap Clipboard::ReadImage(Buffer buffer) const {
  return GetClipboard()->ReadImage();
}

void Clipboard::ReadCustomData(Buffer buffer,
                               const string16& type,
                               string16* result) const {
  GetClipboard()->ReadCustomData(type, result);
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  GetClipboard()->ReadBookmark(title, url);
}

void Clipboard::ReadFile(FilePath* file) const {
  GetClipboard()->ReadFile(file);
}

void Clipboard::ReadFiles(std::vector<FilePath>* files) const {
  GetClipboard()->ReadFiles(files);
}

void Clipboard::ReadData(const FormatType& format, std::string* result) const {
  GetClipboard()->ReadData(format.ToString(), result);
}

uint64 Clipboard::GetSequenceNumber(Buffer buffer) {
  return GetClipboard()->GetNumClipboardEntries();
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  ClipboardDataBuilder::WriteText(text_data, text_len);
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  ClipboardDataBuilder::WriteHTML(markup_data, markup_len, url_data, url_len);
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  ClipboardDataBuilder::WriteBookmark(title_data, title_len, url_data, url_len);
}

void Clipboard::WriteWebSmartPaste() {
  ClipboardDataBuilder::WriteWebSmartPaste();
}

void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  ClipboardDataBuilder::WriteBitmap(pixel_data, size_data);
}

void Clipboard::WriteData(const FormatType& format,
                          const char* data_data,
                          size_t data_len) {
  ClipboardDataBuilder::WriteData(format.ToString(), data_data, data_len);
}

// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
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
const Clipboard::FormatType& Clipboard::GetFilenameFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeFilename));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameWFormatType() {
  return Clipboard::GetFilenameFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeBitmap));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

}  // namespace ui
