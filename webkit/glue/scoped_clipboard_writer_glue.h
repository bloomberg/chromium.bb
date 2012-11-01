// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_SCOPED_CLIPBOARD_WRITER_GLUE_H_
#define WEBKIT_GLUE_SCOPED_CLIPBOARD_WRITER_GLUE_H_

#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/glue/clipboard_client.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WEBKIT_GLUE_EXPORT ScopedClipboardWriterGlue
    : public ui::ScopedClipboardWriter {
 public:
  explicit ScopedClipboardWriterGlue(webkit_glue::ClipboardClient* client);

  virtual ~ScopedClipboardWriterGlue();

  void WriteBitmapFromPixels(const void* pixels, const gfx::Size& size);

 private:
  scoped_ptr<webkit_glue::ClipboardClient::WriteContext> context_;
  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriterGlue);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_SCOPED_CLIPBOARD_WRITER_GLUE_H_
