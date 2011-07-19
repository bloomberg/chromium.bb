// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCOPED_CLIPBOARD_WRITER_GLUE_H_
#define SCOPED_CLIPBOARD_WRITER_GLUE_H_

#include "ui/base/clipboard/scoped_clipboard_writer.h"

class SkBitmap;

namespace base {
class SharedMemory;
}

class ScopedClipboardWriterGlue : public ui::ScopedClipboardWriter {
 public:
   ScopedClipboardWriterGlue(ui::Clipboard* clipboard)
      : ui::ScopedClipboardWriter(clipboard),
        shared_buf_(NULL) {
  }

  ~ScopedClipboardWriterGlue();

  void WriteBitmapFromPixels(const void* pixels, const gfx::Size& size);

 private:
  base::SharedMemory* shared_buf_;
  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriterGlue);
};

#endif  // SCOPED_CLIPBOARD_WRITER_GLUE_H_
