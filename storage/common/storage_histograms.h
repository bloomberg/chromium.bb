// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_STORAGE_HISTOGRAMS_H_
#define STORAGE_COMMON_STORAGE_HISTOGRAMS_H_

#include <string>

#include "base/component_export.h"

namespace storage {

COMPONENT_EXPORT(STORAGE_COMMON)
void RecordBytesWritten(const char* label, int bytes);
COMPONENT_EXPORT(STORAGE_COMMON)
void RecordBytesRead(const char* label, int bytes);

}  // namespace storage

#endif  // STORAGE_COMMON_STORAGE_HISTOGRAMS_H_
