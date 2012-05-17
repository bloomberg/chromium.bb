// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DataPack represents a read-only view onto an on-disk file that contains
// (key, value) pairs of data.  It's used to store static resources like
// translation strings and images.

#ifndef UI_BASE_RESOURCE_DATA_PACK_H_
#define UI_BASE_RESOURCE_DATA_PACK_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "ui/base/ui_export.h"
#include "ui/base/resource/resource_handle.h"

class FilePath;

namespace base {
class RefCountedStaticMemory;
}

namespace file_util {
class MemoryMappedFile;
}

namespace ui {

class UI_EXPORT DataPack : public ResourceHandle {
 public:
  DataPack(float scale_factor);
  virtual ~DataPack();

  // Load a pack file from |path|, returning false on error.
  bool Load(const FilePath& path);

  // Writes a pack file containing |resources| to |path|. If there are any
  // text resources to be written, their encoding must already agree to the
  // |textEncodingType| specified. If no text resources are present, please
  // indicate BINARY.
  static bool WritePack(const FilePath& path,
                        const std::map<uint16, base::StringPiece>& resources,
                        TextEncodingType textEncodingType);

  // ResourceHandle implementation:
  virtual bool GetStringPiece(uint16 resource_id,
                              base::StringPiece* data) const OVERRIDE;
  virtual base::RefCountedStaticMemory* GetStaticMemory(
      uint16 resource_id) const OVERRIDE;
  virtual TextEncodingType GetTextEncodingType() const OVERRIDE;
  virtual float GetScaleFactor() const OVERRIDE;

 private:
  // The memory-mapped data.
  scoped_ptr<file_util::MemoryMappedFile> mmap_;

  // Number of resources in the data.
  size_t resource_count_;

  // Type of encoding for text resources.
  TextEncodingType text_encoding_type_;

  // The scale of the image in this resource pack relative to images in the 1x
  // resource pak.
  float scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(DataPack);
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_DATA_PACK_H_
