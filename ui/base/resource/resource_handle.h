// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_HANDLE_H_
#define UI_BASE_RESOURCE_RESOURCE_HANDLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "ui/base/ui_export.h"

namespace base {
class RefCountedStaticMemory;
}

namespace ui {

class UI_EXPORT ResourceHandle {
 public:
  // What type of encoding the text resources use.
  enum TextEncodingType {
    BINARY,
    UTF8,
    UTF16
  };

  // The scale factors for image resources.
  static const float kScaleFactor100x;
  static const float kScaleFactor200x;

  virtual ~ResourceHandle() {}

  // Get resource by id |resource_id|, filling in |data|.
  // The data is owned by the DataPack object and should not be modified.
  // Returns false if the resource id isn't found.
  virtual bool GetStringPiece(uint16 resource_id,
                              base::StringPiece* data) const = 0;

  // Like GetStringPiece(), but returns a reference to memory.
  // Caller owns the returned object.
  virtual base::RefCountedStaticMemory* GetStaticMemory(
      uint16 resource_id) const = 0;

  // Get the encoding type of text resources.
  virtual TextEncodingType GetTextEncodingType() const = 0;

  // The scale of images in this resource pack relative to images in the 1x
  // resource pak.
  virtual float GetScaleFactor() const = 0;
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_HANDLE_H_
