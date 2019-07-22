// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_
#define UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_

#include <windows.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/resource/resource_handle.h"

namespace ui {

class ResourceDataDLL : public ResourceHandle {
 public:
  explicit ResourceDataDLL(HINSTANCE module);
  ~ResourceDataDLL() override;

  // ResourceHandle implementation:
  bool HasResource(uint16_t resource_id) const override;
  bool GetStringPiece(uint16_t resource_id,
                      base::StringPiece* data) const override;
  base::RefCountedStaticMemory* GetStaticMemory(
      uint16_t resource_id) const override;
  TextEncodingType GetTextEncodingType() const override;
  ScaleFactor GetScaleFactor() const override;

 private:
  const HINSTANCE module_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDataDLL);
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_
