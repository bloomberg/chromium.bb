// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_SKIA_WRAPPER_H_
#define PRINTING_METAFILE_SKIA_WRAPPER_H_

#include "printing/printing_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkCanvas;

namespace printing {

class Metafile;

// A wrapper class with static methods to set and retrieve a Metafile
// on an SkCanvas.  The ownership of the metafile is not affected and it
// is the caller's responsibility to ensure that the metafile remains valid
// as long as the canvas.
class PRINTING_EXPORT MetafileSkiaWrapper : public SkRefCnt {
 public:
  static void SetMetafileOnCanvas(SkCanvas* canvas, Metafile* metafile);

  static Metafile* GetMetafileFromCanvas(SkCanvas* canvas);

  static void SetDraftMode(SkCanvas* canvas, bool draft_mode);

  static bool GetDraftMode(SkCanvas* canvas);

 private:
  explicit MetafileSkiaWrapper(Metafile* metafile);

  Metafile* metafile_;
};

}  // namespace printing

#endif  // PRINTING_METAFILE_SKIA_WRAPPER_H_
