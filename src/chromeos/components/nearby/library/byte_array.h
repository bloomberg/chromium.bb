// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_BYTE_ARRAY_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_BYTE_ARRAY_H_

#include <string>

namespace location {

namespace nearby {

// TODO(kyleqian): Remove this file pending Nearby library import. This is a
// temporary placeholder for the ByteArray class from the Nearby library, which
// is a dependency of HashUtils. See bug #861813 -> https://crbug.com/861813.
class ByteArray {
 public:
  ByteArray() {}
  ByteArray(const char* data, size_t size) { setData(data, size); }

  void setData(const char* data, size_t size) { data_.assign(data, size); }
  const char* getData() const { return data_.data(); }
  size_t size() const { return data_.size(); }

 private:
  std::string data_;
};

}  // namespace nearby

}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_BYTE_ARRAY_H_
