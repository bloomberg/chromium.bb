// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkMetaData.h"

namespace printing {

namespace {

const char* kMetafileKey = "CrMetafile";

}  // namespace

// static
void MetafileSkiaWrapper::SetMetafileOnCanvas(const SkCanvas& canvas,
                                              PdfMetafileSkia* metafile) {
  skia::RefPtr<MetafileSkiaWrapper> wrapper;
  if (metafile)
    wrapper = skia::AdoptRef(new MetafileSkiaWrapper(metafile));

  SkMetaData& meta = skia::GetMetaData(canvas);
  meta.setRefCnt(kMetafileKey, wrapper.get());
}

// static
PdfMetafileSkia* MetafileSkiaWrapper::GetMetafileFromCanvas(
    const SkCanvas& canvas) {
  SkMetaData& meta = skia::GetMetaData(canvas);
  SkRefCnt* value;
  if (!meta.findRefCnt(kMetafileKey, &value) || !value)
    return NULL;

  return static_cast<MetafileSkiaWrapper*>(value)->metafile_;
}

MetafileSkiaWrapper::MetafileSkiaWrapper(PdfMetafileSkia* metafile)
    : metafile_(metafile) {
}

}  // namespace printing
