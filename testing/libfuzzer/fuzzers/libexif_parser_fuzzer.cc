// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "libexif/exif-data.h"
#include "libexif/exif-system.h"

extern "C" int LLVMFuzzerTestOneInput(unsigned char* data, size_t size) {
  ExifData* exif_data = exif_data_new_from_data(data, size);
  exif_data_unref(exif_data);
  return 0;
}
