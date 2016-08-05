// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/os_exchange_data_provider_mus.h"

namespace views {

OSExchangeDataProviderMus::OSExchangeDataProviderMus() {}

OSExchangeDataProviderMus::~OSExchangeDataProviderMus() {}

std::unique_ptr<ui::OSExchangeData::Provider>
OSExchangeDataProviderMus::Clone() const {
  return std::unique_ptr<ui::OSExchangeData::Provider>();
}

void OSExchangeDataProviderMus::MarkOriginatedFromRenderer() {
}

bool OSExchangeDataProviderMus::DidOriginateFromRenderer() const {
  return false;
}

void OSExchangeDataProviderMus::SetString(const base::string16& data) {
}

void OSExchangeDataProviderMus::SetURL(const GURL& url,
                                       const base::string16& title) {
}

void OSExchangeDataProviderMus::SetFilename(const base::FilePath& path) {
}

void OSExchangeDataProviderMus::SetFilenames(
    const std::vector<ui::FileInfo>& file_names) {
}

void OSExchangeDataProviderMus::SetPickledData(
    const ui::Clipboard::FormatType& format,
    const base::Pickle& data) {
}

bool OSExchangeDataProviderMus::GetString(base::string16* data) const {
  return false;
}

bool OSExchangeDataProviderMus::GetURLAndTitle(
    ui::OSExchangeData::FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  return false;
}

bool OSExchangeDataProviderMus::GetFilename(base::FilePath* path) const {
  return false;
}

bool OSExchangeDataProviderMus::GetFilenames(
    std::vector<ui::FileInfo>* file_names) const {
  return false;
}

bool OSExchangeDataProviderMus::GetPickledData(
    const ui::Clipboard::FormatType& format,
    base::Pickle* data) const {
  return false;
}

bool OSExchangeDataProviderMus::HasString() const {
  return false;
}

bool OSExchangeDataProviderMus::HasURL(
    ui::OSExchangeData::FilenameToURLPolicy policy) const {
  return false;
}

bool OSExchangeDataProviderMus::HasFile() const {
  return false;
}

bool OSExchangeDataProviderMus::HasCustomFormat(
    const ui::Clipboard::FormatType& format) const {
  return false;
}

// These methods were added in an ad-hoc way to different operating
// systems. We need to support them until they get cleaned up.
#if (!defined(OS_CHROMEOS) && defined(USE_X11)) || defined(OS_WIN)
void OSExchangeDataProviderMus::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {

}
#endif

#if defined(OS_WIN)
bool OSExchangeDataProviderMus::GetFileContents(
    base::FilePath* filename,
    std::string* file_contents) const {
  return false;
}

bool OSExchangeDataProviderMus::HasFileContents() const {
  return false;
}

void OSExchangeDataProviderMus::SetDownloadFileInfo(
    const ui::OSExchangeData::DownloadFileInfo& download) {
}
#endif

#if defined(USE_AURA)
void OSExchangeDataProviderMus::SetHtml(const base::string16& html,
                                        const GURL& base_url) {

}

bool OSExchangeDataProviderMus::GetHtml(base::string16* html,
                                        GURL* base_url) const {
  return false;
}

bool OSExchangeDataProviderMus::HasHtml() const {
  return false;
}
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
void OSExchangeDataProviderMus::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

const gfx::ImageSkia& OSExchangeDataProviderMus::GetDragImage() const {
  return drag_image_;
}

const gfx::Vector2d& OSExchangeDataProviderMus::GetDragImageOffset() const {
  return drag_image_offset_;
}
#endif

}  // namespace views
