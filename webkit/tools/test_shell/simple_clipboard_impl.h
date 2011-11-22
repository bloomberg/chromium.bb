// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_CLIPBOARD_IMPL_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_CLIPBOARD_IMPL_H_

#include "webkit/glue/clipboard_client.h"

class SimpleClipboardClient : public webkit_glue::ClipboardClient {
 public:
  SimpleClipboardClient();
  virtual ~SimpleClipboardClient();

  virtual ui::Clipboard* GetClipboard();
  virtual uint64 GetSequenceNumber(ui::Clipboard::Buffer buffer);
  virtual bool IsFormatAvailable(const ui::Clipboard::FormatType& format,
                                 ui::Clipboard::Buffer buffer);
  virtual void ReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                  std::vector<string16>* types,
                                  bool* contains_filenames);
  virtual void ReadText(ui::Clipboard::Buffer buffer, string16* result);
  virtual void ReadAsciiText(ui::Clipboard::Buffer buffer,
                             std::string* result);
  virtual void ReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                        GURL* url, uint32* fragment_start,
                        uint32* fragment_end);
  virtual void ReadImage(ui::Clipboard::Buffer buffer, std::string* data);
  virtual WriteContext* CreateWriteContext();
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_CLIPBOARD_IMPL_H_
