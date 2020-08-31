// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_
#define UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

// Platform-neutral ui::Clipboard mock used for tests.
class TestClipboard : public Clipboard {
 public:
  TestClipboard();
  ~TestClipboard() override;

  // Creates and associates a TestClipboard with the current thread. When no
  // longer needed, the returned clipboard must be freed by calling
  // Clipboard::DestroyClipboardForCurrentThread() on the same thread.
  static TestClipboard* CreateForCurrentThread();

  // Sets the time to be returned by GetLastModifiedTime();
  void SetLastModifiedTime(const base::Time& time);

  // Clipboard overrides.
  void OnPreShutdown() override;
  uint64_t GetSequenceNumber(ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          std::vector<base::string16>* types) const override;
  std::vector<base::string16> ReadAvailablePlatformSpecificFormatNames(
      ClipboardBuffer buffer) const override;
  void ReadText(ClipboardBuffer buffer, base::string16* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadRTF(ClipboardBuffer buffer, std::string* result) const override;
  void ReadImage(ClipboardBuffer buffer,
                 ReadImageCallback callback) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const base::string16& type,
                      base::string16* result) const override;
  void ReadBookmark(base::string16* title, std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                std::string* result) const override;
  base::Time GetLastModifiedTime() const override;
  void ClearLastModifiedTime() override;
  void WritePortableRepresentations(ClipboardBuffer buffer,
                                    const ObjectMap& objects) override;
  void WritePlatformRepresentations(
      ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation> platform_representations)
      override;
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

 private:
  struct DataStore {
    DataStore();
    DataStore(const DataStore& other);
    ~DataStore();
    void Clear();
    uint64_t sequence_number;
    base::flat_map<ClipboardFormatType, std::string> data;
    std::string url_title;
    std::string html_src_url;
    SkBitmap image;
  };

  // The non-const versions increment the sequence number as a side effect.
  const DataStore& GetStore(ClipboardBuffer buffer) const;
  const DataStore& GetDefaultStore() const;
  DataStore& GetStore(ClipboardBuffer buffer);
  DataStore& GetDefaultStore();

  ClipboardBuffer default_store_buffer_;
  mutable base::flat_map<ClipboardBuffer, DataStore> stores_;
  base::Time last_modified_time_;

  DISALLOW_COPY_AND_ASSIGN(TestClipboard);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_TEST_TEST_CLIPBOARD_H_
