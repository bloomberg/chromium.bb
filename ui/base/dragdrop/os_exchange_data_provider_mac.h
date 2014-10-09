// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_

#import "base/mac/scoped_nsobject.h"
#include "ui/base/dragdrop/os_exchange_data.h"

@class NSPasteboard;

namespace ui {

// OSExchangeData::Provider implementation for Mac.
class UI_BASE_EXPORT OSExchangeDataProviderMac
    : public OSExchangeData::Provider {
 public:
  OSExchangeDataProviderMac();
  explicit OSExchangeDataProviderMac(NSPasteboard* pasteboard);
  virtual ~OSExchangeDataProviderMac();

  // Overridden from OSExchangeData::Provider:
  virtual Provider* Clone() const override;
  virtual void MarkOriginatedFromRenderer() override;
  virtual bool DidOriginateFromRenderer() const override;
  virtual void SetString(const base::string16& data) override;
  virtual void SetURL(const GURL& url, const base::string16& title) override;
  virtual void SetFilename(const base::FilePath& path) override;
  virtual void SetFilenames(const std::vector<FileInfo>& filenames) override;
  virtual void SetPickledData(const OSExchangeData::CustomFormat& format,
                              const Pickle& data) override;
  virtual bool GetString(base::string16* data) const override;
  virtual bool GetURLAndTitle(OSExchangeData::FilenameToURLPolicy policy,
                              GURL* url,
                              base::string16* title) const override;
  virtual bool GetFilename(base::FilePath* path) const override;
  virtual bool GetFilenames(std::vector<FileInfo>* filenames) const override;
  virtual bool GetPickledData(const OSExchangeData::CustomFormat& format,
                              Pickle* data) const override;
  virtual bool HasString() const override;
  virtual bool HasURL(
      OSExchangeData::FilenameToURLPolicy policy) const override;
  virtual bool HasFile() const override;
  virtual bool HasCustomFormat(
      const OSExchangeData::CustomFormat& format) const override;

 private:
  base::scoped_nsobject<NSPasteboard> pasteboard_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderMac);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
