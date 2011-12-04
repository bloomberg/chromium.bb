// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CLIPBOARD_CLIENT_H_
#define WEBKIT_GLUE_CLIPBOARD_CLIENT_H_

#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace webkit_glue {

// Interface for the webkit glue embedder to implement to support clipboard.
class ClipboardClient {
 public:
  class WriteContext {
   public:
    virtual ~WriteContext() { }

    // Writes bitmap data into the context, updating the ObjectMap.
    virtual void WriteBitmapFromPixels(ui::Clipboard::ObjectMap* objects,
                                       const void* pixels,
                                       const gfx::Size& size) = 0;

    // Flushes all gathered data, and destroys the context.
    virtual void FlushAndDestroy(const ui::Clipboard::ObjectMap& objects) = 0;
  };

  virtual ~ClipboardClient() { }

  // Get a clipboard that can be used to construct a ScopedClipboardWriterGlue.
  virtual ui::Clipboard* GetClipboard() = 0;

  // Get a sequence number which uniquely identifies clipboard state.
  virtual uint64 GetSequenceNumber(ui::Clipboard::Buffer buffer) = 0;

  // Tests whether the clipboard contains a certain format
  virtual bool IsFormatAvailable(const ui::Clipboard::FormatType& format,
                                 ui::Clipboard::Buffer buffer) = 0;

  // Reads the available types from the clipboard, if available.
  virtual void ReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                  std::vector<string16>* types,
                                  bool* contains_filenames) = 0;

  // Reads UNICODE text from the clipboard, if available.
  virtual void ReadText(ui::Clipboard::Buffer buffer, string16* result) = 0;

  // Reads ASCII text from the clipboard, if available.
  virtual void ReadAsciiText(ui::Clipboard::Buffer buffer,
                             std::string* result) = 0;

  // Reads HTML from the clipboard, if available.
  virtual void ReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                        GURL* url, uint32* fragment_start,
                        uint32* fragment_end) = 0;

  // Reads and image from the clipboard, if available.
  virtual void ReadImage(ui::Clipboard::Buffer buffer, std::string* data) = 0;

  // Reads a custom data type from the clipboard, if available.
  virtual void ReadCustomData(ui::Clipboard::Buffer buffer,
                              const string16& type,
                              string16* data) = 0;

  // Creates a context to write clipboard data. May return NULL.
  virtual WriteContext* CreateWriteContext() = 0;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_CLIPBOARD_CLIENT_H_
