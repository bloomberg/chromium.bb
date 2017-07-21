// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DataPack represents a read-only view onto an on-disk file that contains
// (key, value) pairs of data.  It's used to store static resources like
// translation strings and images.

#ifndef UI_BASE_RESOURCE_DATA_PACK_H_
#define UI_BASE_RESOURCE_DATA_PACK_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "ui/base/resource/data_pack_export.h"
#include "ui/base/resource/resource_handle.h"

namespace base {
class FilePath;
class RefCountedStaticMemory;
}

namespace ui {
enum ScaleFactor : int;

class UI_DATA_PACK_EXPORT DataPack : public ResourceHandle {
 public:
  explicit DataPack(ui::ScaleFactor scale_factor);
  ~DataPack() override;

  // Load a pack file from |path|, returning false on error.
  bool LoadFromPath(const base::FilePath& path);

  // Loads a pack file from |file|, returning false on error.
  bool LoadFromFile(base::File file);

  // Loads a pack file from |region| of |file|, returning false on error.
  bool LoadFromFileRegion(base::File file,
                          const base::MemoryMappedFile::Region& region);

  // Loads a pack file from |buffer|, returning false on error.
  // Data is not copied, |buffer| should stay alive during |DataPack| lifetime.
  bool LoadFromBuffer(base::StringPiece buffer);

  // Writes a pack file containing |resources| to |path|. If there are any
  // text resources to be written, their encoding must already agree to the
  // |textEncodingType| specified. If no text resources are present, please
  // indicate BINARY.
  static bool WritePack(const base::FilePath& path,
                        const std::map<uint16_t, base::StringPiece>& resources,
                        TextEncodingType textEncodingType);

  // ResourceHandle implementation:
  bool HasResource(uint16_t resource_id) const override;
  bool GetStringPiece(uint16_t resource_id,
                      base::StringPiece* data) const override;
  base::RefCountedStaticMemory* GetStaticMemory(
      uint16_t resource_id) const override;
  TextEncodingType GetTextEncodingType() const override;
  ui::ScaleFactor GetScaleFactor() const override;

#if DCHECK_IS_ON()
  // Checks to see if any resource in this DataPack already exists in the list
  // of resources.
  void CheckForDuplicateResources(
      const std::vector<std::unique_ptr<ResourceHandle>>& packs);
#endif

 private:
  class DataSource;
  class BufferDataSource;
  class MemoryMappedDataSource;

  // Does the actual loading of a pack file.
  // Called by Load and LoadFromFile and LoadFromBuffer.
  bool LoadImpl(std::unique_ptr<DataSource> data_source);

  std::unique_ptr<DataSource> data_source_;

  // Number of resources in the data.
  size_t resource_count_;

  // Type of encoding for text resources.
  TextEncodingType text_encoding_type_;

  // The scale of the image in this resource pack relative to images in the 1x
  // resource pak.
  ui::ScaleFactor scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(DataPack);
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_DATA_PACK_H_
