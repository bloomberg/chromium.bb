// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "base/logging.h"

ScopedClipboardWriterGlue::ScopedClipboardWriterGlue(
    webkit_glue::ClipboardClient* client)
    : ui::ScopedClipboardWriter(client->GetClipboard()),
      context_(client->CreateWriteContext()) {
  // We should never have an instance where both are set.
  DCHECK((clipboard_ && !context_) ||
         (!clipboard_ && context_));
}

ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
  if (!objects_.empty() && context_) {
    context_->FlushAndDestroy(objects_);
  }
}

void ScopedClipboardWriterGlue::WriteBitmapFromPixels(const void* pixels,
                                                      const gfx::Size& size) {
  if (context_) {
    context_->WriteBitmapFromPixels(&objects_, pixels, size);
  } else {
    ScopedClipboardWriter::WriteBitmapFromPixels(pixels, size);
  }
}
