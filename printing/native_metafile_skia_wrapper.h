// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_SKIA_WRAPPER_H_
#define PRINTING_NATIVE_METAFILE_SKIA_WRAPPER_H_

#include "third_party/skia/include/core/SkRefCnt.h"

class SkCanvas;

namespace printing {

class NativeMetafile;

// A wrapper class with static methods to set and retrieve a NativeMetafile
// on an SkCanvas.  The ownership of the metafile is not affected and it
// is the caller's responsibility to ensure that the metafile remains valid
// as long as the canvas.
class NativeMetafileSkiaWrapper : public SkRefCnt {
 public:
  static void SetMetafileOnCanvas(SkCanvas* canvas, NativeMetafile* metafile);

  static NativeMetafile* GetMetafileFromCanvas(SkCanvas* canvas);

 private:
  explicit NativeMetafileSkiaWrapper(NativeMetafile* metafile);

  NativeMetafile* metafile_;
};

}  // namespace printing

#endif  // PRINTING_NATIVE_METAFILE_SKIA_WRAPPER_H_
