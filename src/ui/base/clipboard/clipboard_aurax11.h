// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_AURAX11_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_AURAX11_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

class ClipboardAuraX11 : public Clipboard {
 private:
  friend class Clipboard;

  ClipboardAuraX11();
  ~ClipboardAuraX11() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  uint64_t GetSequenceNumber(ClipboardType type) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardType type) const override;
  void Clear(ClipboardType type) override;
  void ReadAvailableTypes(ClipboardType type,
                          std::vector<base::string16>* types,
                          bool* contains_filenames) const override;
  void ReadText(ClipboardType type, base::string16* result) const override;
  void ReadAsciiText(ClipboardType type, std::string* result) const override;
  void ReadHTML(ClipboardType type,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadRTF(ClipboardType type, std::string* result) const override;
  SkBitmap ReadImage(ClipboardType type) const override;
  void ReadCustomData(ClipboardType clipboard_type,
                      const base::string16& type,
                      base::string16* result) const override;
  void ReadBookmark(base::string16* title, std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                std::string* result) const override;
  void WriteObjects(ClipboardType type, const ObjectMap& objects) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 const char* data_data,
                 size_t data_len) override;

  // TODO(dcheng): Is this still needed now that each platform clipboard has its
  // own class derived from Clipboard?
  class AuraX11Details;
  std::unique_ptr<AuraX11Details> aurax11_details_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardAuraX11);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_AURAX11_H_
