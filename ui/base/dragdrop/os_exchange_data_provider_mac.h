// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/dragdrop/os_exchange_data.h"

@class NSPasteboard;

namespace ui {
class UniquePasteboard;

// OSExchangeData::Provider implementation for Mac.
class UI_BASE_EXPORT OSExchangeDataProviderMac
    : public OSExchangeData::Provider {
 public:
  OSExchangeDataProviderMac();
  ~OSExchangeDataProviderMac() override;

  // Overridden from OSExchangeData::Provider:
  Provider* Clone() const override;
  void MarkOriginatedFromRenderer() override;
  bool DidOriginateFromRenderer() const override;
  void SetString(const base::string16& data) override;
  void SetURL(const GURL& url, const base::string16& title) override;
  void SetFilename(const base::FilePath& path) override;
  void SetFilenames(const std::vector<FileInfo>& filenames) override;
  void SetPickledData(const Clipboard::FormatType& format,
                      const base::Pickle& data) override;
  bool GetString(base::string16* data) const override;
  bool GetURLAndTitle(OSExchangeData::FilenameToURLPolicy policy,
                      GURL* url,
                      base::string16* title) const override;
  bool GetFilename(base::FilePath* path) const override;
  bool GetFilenames(std::vector<FileInfo>* filenames) const override;
  bool GetPickledData(const Clipboard::FormatType& format,
                      base::Pickle* data) const override;
  bool HasString() const override;
  bool HasURL(OSExchangeData::FilenameToURLPolicy policy) const override;
  bool HasFile() const override;
  bool HasCustomFormat(const Clipboard::FormatType& format) const override;

 private:
  explicit OSExchangeDataProviderMac(scoped_refptr<ui::UniquePasteboard>);
  scoped_refptr<ui::UniquePasteboard> pasteboard_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderMac);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
