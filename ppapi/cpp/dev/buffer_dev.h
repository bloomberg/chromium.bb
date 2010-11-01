// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_BUFFER_DEV_H_
#define PPAPI_CPP_DEV_BUFFER_DEV_H_

#include "ppapi/cpp/resource.h"

namespace pp {

class Buffer_Dev : public Resource {
 public:
  // Creates an is_null() Buffer object.
  Buffer_Dev();

  Buffer_Dev(const Buffer_Dev& other);

  // Allocates a new Buffer in the browser with the given size. The
  // resulting object will be is_null() if the allocation failed.
  explicit Buffer_Dev(int32_t size);

  ~Buffer_Dev();

  Buffer_Dev& operator=(const Buffer_Dev& other);
  void swap(Buffer_Dev& other);

  int32_t size() const { return size_; }
  void* data() const { return data_; }

 private:
  void* data_;
  int32_t size_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_BUFFER_DEV_H_

