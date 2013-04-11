// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_PICASA_PMP_TEST_HELPER_H_
#define WEBKIT_FILEAPI_MEDIA_PICASA_PMP_TEST_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "webkit/fileapi/media/picasa/pmp_constants.h"

namespace base {
class FilePath;
}  // namespace base

namespace picasaimport {

class PmpColumnReader;

// A helper class used for unit tests only
class PmpTestHelper {
 public:
  PmpTestHelper();

  bool Init();

  base::FilePath GetTempDirPath();

  template<class T>
  bool WriteColumnFileFromVector(const std::string& table_name,
                                 const std::string& column_name,
                                 const PmpFieldType field_type,
                                 const std::vector<T>& elements_vector);

  bool InitColumnReaderFromBytes(PmpColumnReader* const reader,
                                 const std::vector<uint8>& data,
                                 uint32* rows_read);

  static std::vector<uint8> MakeHeader(const PmpFieldType field_type,
                                       const uint32 row_count);

  template<class T>
  static std::vector<uint8> MakeHeaderAndBody(const PmpFieldType field_type,
                                              const uint32 row_count,
                                              const std::vector<T>& elems);

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace picasaimport

#endif  // WEBKIT_FILEAPI_MEDIA_PICASA_PMP_TEST_HELPER_H_
