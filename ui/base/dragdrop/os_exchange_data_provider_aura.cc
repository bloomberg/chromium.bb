// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"

namespace ui {

OSExchangeDataProviderAura::OSExchangeDataProviderAura() : formats_(0) {}

OSExchangeDataProviderAura::~OSExchangeDataProviderAura() {}

void OSExchangeDataProviderAura::SetString(const string16& data) {
  string_ = data;
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderAura::SetURL(const GURL& url,
                                        const string16& title) {
  url_ = url;
  title_ = title;
  formats_ |= OSExchangeData::URL;
}

void OSExchangeDataProviderAura::SetFilename(const FilePath& path) {
  filename_ = path;
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderAura::SetPickledData(
    OSExchangeData::CustomFormat format,
    const Pickle& data) {
  pickle_data_[format] = data;
  formats_ |= OSExchangeData::PICKLED_DATA;
}

bool OSExchangeDataProviderAura::GetString(string16* data) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;
  *data = string_;
  return true;
}

bool OSExchangeDataProviderAura::GetURLAndTitle(GURL* url,
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

bool OSExchangeDataProviderAura::GetFilename(FilePath* path) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  *path = filename_;
  return true;
}

bool OSExchangeDataProviderAura::GetPickledData(
    OSExchangeData::CustomFormat format,
    Pickle* data) const {
  PickleData::const_iterator i = pickle_data_.find(format);
  if (i == pickle_data_.end())
    return false;

  *data = i->second;
  return true;
}

bool OSExchangeDataProviderAura::HasString() const {
  return (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderAura::HasURL() const {
  if ((formats_ & OSExchangeData::URL) != 0) {
    return true;
  }
  // No URL, see if we have plain text that can be parsed as a URL.
  return GetPlainTextURL(NULL);
}

bool OSExchangeDataProviderAura::HasFile() const {
  return (formats_ & OSExchangeData::FILE_NAME) != 0;
}

bool OSExchangeDataProviderAura::HasCustomFormat(
    OSExchangeData::CustomFormat format) const {
  return pickle_data_.find(format) != pickle_data_.end();
}

#if defined(OS_WIN)
  void OSExchangeDataProviderAura::SetFileContents(
      const FilePath& filename,
      const std::string& file_contents) {
    NOTIMPLEMENTED();
  }

  void OSExchangeDataProviderAura::SetHtml(const string16& html,
                                           const GURL& base_url) {
    NOTIMPLEMENTED();
  }

  bool OSExchangeDataProviderAura::GetFileContents(
      FilePath* filename,
      std::string* file_contents) const {
    NOTIMPLEMENTED();
    return false;
  }

  bool OSExchangeDataProviderAura::GetHtml(string16* html,
                                           GURL* base_url) const {
    NOTIMPLEMENTED();
    return false;
  }

  bool OSExchangeDataProviderAura::HasFileContents() const {
    NOTIMPLEMENTED();
    return false;
  }

  bool OSExchangeDataProviderAura::HasHtml() const {
    NOTIMPLEMENTED();
    return false;
  }

  void OSExchangeDataProviderAura::SetDownloadFileInfo(
      const OSExchangeData::DownloadFileInfo& download) {
    NOTIMPLEMENTED();
  }
#endif

bool OSExchangeDataProviderAura::GetPlainTextURL(GURL* url) const {
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
  return new OSExchangeDataProviderAura();
}

// static
OSExchangeData::CustomFormat
OSExchangeData::RegisterCustomFormat(const std::string& type) {
  // TODO(davemoore) Implement this for aura.
  return NULL;
}

}  // namespace ui
