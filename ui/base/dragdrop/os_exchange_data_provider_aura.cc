// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data.h"

#include "base/logging.h"

namespace ui {

// OSExchangeData::Provider implementation for aura on linux.
class OSExchangeDataProviderAura : public OSExchangeData::Provider {
 public:
  OSExchangeDataProviderAura() {
    NOTIMPLEMENTED();
  }

  virtual ~OSExchangeDataProviderAura() {
  }

  virtual void SetString(const string16& data) OVERRIDE {
  }
  virtual void SetURL(const GURL& url, const string16& title) OVERRIDE {
  }
  virtual void SetFilename(const FilePath& path) OVERRIDE {
  }
  virtual void SetPickledData(OSExchangeData::CustomFormat format,
                              const Pickle& data) OVERRIDE {
  }

  virtual bool GetString(string16* data) const OVERRIDE {
    return false;
  }
  virtual bool GetURLAndTitle(GURL* url, string16* title) const OVERRIDE {
    return false;
  }
  virtual bool GetFilename(FilePath* path) const OVERRIDE {
    return false;
  }
  virtual bool GetPickledData(OSExchangeData::CustomFormat format,
                              Pickle* data) const OVERRIDE {
    return false;
  }

  virtual bool HasString() const OVERRIDE {
    return false;
  }
  virtual bool HasURL() const OVERRIDE {
    return false;
  }
  virtual bool HasFile() const OVERRIDE {
    return false;
  }
  virtual bool HasCustomFormat(
      OSExchangeData::CustomFormat format) const OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderAura);
};

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderAura();
}

}  // namespace ui
