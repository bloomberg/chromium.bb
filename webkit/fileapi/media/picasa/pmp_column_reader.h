// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_PICASA_PMP_COLUMN_READER_H_
#define WEBKIT_FILEAPI_MEDIA_PICASA_PMP_COLUMN_READER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/media/picasa/pmp_constants.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class FilePath;
}

namespace picasaimport {

// Reads a single PMP column from a file.
class WEBKIT_STORAGE_EXPORT_PRIVATE PmpColumnReader {
 public:
  PmpColumnReader();
  virtual ~PmpColumnReader();

  // Returns true if read successfully.
  // |rows_read| is undefined if returns false.
  bool Init(const base::FilePath& filepath, uint32* rows_read);

  // These functions read the value of that |row| into |result|.
  // Functions return false if the column is of the wrong type or the row
  // is out of range.
  bool ReadString(const uint32 row, std::string* result) const;
  bool ReadUInt32(const uint32 row, uint32* result) const;
  bool ReadDouble64(const uint32 row, double* result) const;
  bool ReadUInt8(const uint32 row, uint8* result) const;
  bool ReadUInt64(const uint32 row, uint64* result) const;

  // Returns the native encoding of field_type.
  PmpFieldType field_type() const {
    return field_type_;
  }

 private:
  bool ParseData(uint32* rows_read);
  // Returns the number of bytes parsed in the body, or, -1 on failure.
  long IndexStrings();

  // Source data
  scoped_array<uint8> data_;
  size_t length_;

  // Header data
  PmpFieldType field_type_;
  uint32 rows_;

  // Index of string start locations if fields are strings. Empty otherwise.
  std::vector<const char*> strings_;

  DISALLOW_COPY_AND_ASSIGN(PmpColumnReader);
};

}  // namespace picasaimport

#endif  // WEBKIT_FILEAPI_MEDIA_PICASA_PMP_COLUMN_READER_H_
