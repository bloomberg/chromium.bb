// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "printing/native_metafile_skia_wrapper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkRefDict.h"

namespace printing {

namespace {

static const char* kNativeMetafileKey = "CrNativeMetafile";

SkRefDict& getRefDict(SkCanvas* canvas) {
  DCHECK(canvas != NULL);

  SkDevice* device = canvas->getDevice();
  DCHECK(device != NULL);
  return device->getRefDict();
}

}  // namespace


// static
void NativeMetafileSkiaWrapper::SetMetafileOnCanvas(SkCanvas* canvas,
                                                    NativeMetafile* metafile) {
  NativeMetafileSkiaWrapper* wrapper = NULL;
  if (metafile)
    wrapper = new NativeMetafileSkiaWrapper(metafile);

  SkRefDict& dict = getRefDict(canvas);
  dict.set(kNativeMetafileKey, wrapper);
  SkSafeUnref(wrapper);
}

// static
NativeMetafile* NativeMetafileSkiaWrapper::GetMetafileFromCanvas(
    SkCanvas* canvas) {
  SkRefDict& dict = getRefDict(canvas);
  SkRefCnt* value = dict.find(kNativeMetafileKey);
  if (!value)
    return NULL;

  return static_cast<NativeMetafileSkiaWrapper*>(value)->metafile_;
}

NativeMetafileSkiaWrapper::NativeMetafileSkiaWrapper(NativeMetafile* metafile)
    : metafile_(metafile) {
}

}  // namespace printing
