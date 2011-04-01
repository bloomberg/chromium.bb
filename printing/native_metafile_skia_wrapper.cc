// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "printing/native_metafile_skia_wrapper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkMetaData.h"

namespace printing {

namespace {

static const char* kNativeMetafileKey = "CrNativeMetafile";

SkMetaData& getMetaData(SkCanvas* canvas) {
  DCHECK(canvas != NULL);

  SkDevice* device = canvas->getDevice();
  DCHECK(device != NULL);
  return device->getMetaData();
}

}  // namespace


// static
void NativeMetafileSkiaWrapper::SetMetafileOnCanvas(SkCanvas* canvas,
                                                    NativeMetafile* metafile) {
  NativeMetafileSkiaWrapper* wrapper = NULL;
  if (metafile)
    wrapper = new NativeMetafileSkiaWrapper(metafile);

  SkMetaData& meta = getMetaData(canvas);
  meta.setRefCnt(kNativeMetafileKey, wrapper);
  SkSafeUnref(wrapper);
}

// static
NativeMetafile* NativeMetafileSkiaWrapper::GetMetafileFromCanvas(
    SkCanvas* canvas) {
  SkMetaData& meta = getMetaData(canvas);
  SkRefCnt* value;
  if (!meta.findRefCnt(kNativeMetafileKey, &value) || !value)
    return NULL;

  return static_cast<NativeMetafileSkiaWrapper*>(value)->metafile_;
}

NativeMetafileSkiaWrapper::NativeMetafileSkiaWrapper(NativeMetafile* metafile)
    : metafile_(metafile) {
}

}  // namespace printing
