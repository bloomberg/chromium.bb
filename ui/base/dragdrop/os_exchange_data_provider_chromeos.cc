// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_chromeos.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace ui {

OSExchangeDataProviderChromeos::OSExchangeDataProviderChromeos()
    : formats_(0) {
}

OSExchangeDataProviderChromeos::~OSExchangeDataProviderChromeos() {}

void OSExchangeDataProviderChromeos::SetString(const string16& data) {
  string_ = data;
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderChromeos::SetURL(const GURL& url,
                                        const string16& title) {
  url_ = url;
  title_ = title;
  formats_ |= OSExchangeData::URL;
}

void OSExchangeDataProviderChromeos::SetFilename(const base::FilePath& path) {
  filenames_.clear();
  filenames_.push_back(OSExchangeData::FileInfo(path, base::FilePath()));
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderChromeos::SetFilenames(
    const std::vector<OSExchangeData::FileInfo>& filenames) {
  filenames_ = filenames;
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderChromeos::SetPickledData(
    OSExchangeData::CustomFormat format,
    const Pickle& data) {
  pickle_data_[format] = data;
  formats_ |= OSExchangeData::PICKLED_DATA;
}

bool OSExchangeDataProviderChromeos::GetString(string16* data) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;
  *data = string_;
  return true;
}

bool OSExchangeDataProviderChromeos::GetURLAndTitle(GURL* url,
                                                string16* title) const {
  if ((formats_ & OSExchangeData::URL) == 0) {
    title->clear();
    return GetPlainTextURL(url);
  }

  if (!url_.is_valid())
    return false;

  *url = url_;
  *title = title_;
  return true;
}

bool OSExchangeDataProviderChromeos::GetFilename(base::FilePath* path) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  DCHECK(!filenames_.empty());
  *path = filenames_[0].path;
  return true;
}

bool OSExchangeDataProviderChromeos::GetFilenames(
    std::vector<OSExchangeData::FileInfo>* filenames) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  *filenames = filenames_;
  return true;
}

bool OSExchangeDataProviderChromeos::GetPickledData(
    OSExchangeData::CustomFormat format,
    Pickle* data) const {
  PickleData::const_iterator i = pickle_data_.find(format);
  if (i == pickle_data_.end())
    return false;

  *data = i->second;
  return true;
}

bool OSExchangeDataProviderChromeos::HasString() const {
  return (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderChromeos::HasURL() const {
  if ((formats_ & OSExchangeData::URL) != 0) {
    return true;
  }
  // No URL, see if we have plain text that can be parsed as a URL.
  return GetPlainTextURL(NULL);
}

bool OSExchangeDataProviderChromeos::HasFile() const {
  return (formats_ & OSExchangeData::FILE_NAME) != 0;
}

bool OSExchangeDataProviderChromeos::HasCustomFormat(
    OSExchangeData::CustomFormat format) const {
  return pickle_data_.find(format) != pickle_data_.end();
}

void OSExchangeDataProviderChromeos::SetHtml(const string16& html,
                                         const GURL& base_url) {
  formats_ |= OSExchangeData::HTML;
  html_ = html;
  base_url_ = base_url;
}

bool OSExchangeDataProviderChromeos::GetHtml(string16* html,
                                         GURL* base_url) const {
  if ((formats_ & OSExchangeData::HTML) == 0)
    return false;
  *html = html_;
  *base_url = base_url_;
  return true;
}

bool OSExchangeDataProviderChromeos::HasHtml() const {
  return ((formats_ & OSExchangeData::HTML) != 0);
}

void OSExchangeDataProviderChromeos::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

const gfx::ImageSkia& OSExchangeDataProviderChromeos::GetDragImage() const {
  return drag_image_;
}

const gfx::Vector2d&
OSExchangeDataProviderChromeos::GetDragImageOffset() const {
  return drag_image_offset_;
}

bool OSExchangeDataProviderChromeos::GetPlainTextURL(GURL* url) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;

  GURL test_url(string_);
  if (!test_url.is_valid())
    return false;

  if (url)
    *url = test_url;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderChromeos();
}

// static
OSExchangeData::CustomFormat
OSExchangeData::RegisterCustomFormat(const std::string& type) {
  // On Aura you probably want to just use the Clipboard::Get*FormatType APIs
  // instead.  But we can also dynamically generate new CustomFormat objects
  // here too if really necessary.
  return Clipboard::FormatType::Deserialize(type);
}


}  // namespace ui
