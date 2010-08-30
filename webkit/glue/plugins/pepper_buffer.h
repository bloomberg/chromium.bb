// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_BUFFER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_BUFFER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PPB_Buffer_Dev;

namespace pepper {

class PluginInstance;

class Buffer : public Resource {
 public:
  explicit Buffer(PluginModule* module);
  virtual ~Buffer();

  int size() const { return size_; }
  unsigned char* mapped_buffer() { return mem_buffer_.get(); }

  // Returns true if this buffer is mapped. False means that the buffer is
  // either invalid or not mapped.
  bool is_mapped() const { return !!mem_buffer_.get(); }

  // Returns a pointer to the interface implementing PPB_Buffer that is
  // exposed to the plugin.
  static const PPB_Buffer_Dev* GetInterface();

  // Resource overrides.
  Buffer* AsBuffer() { return this; }

  // PPB_Buffer implementation.
  bool Init(int size);
  void Describe(int* size_in_bytes) const;
  void* Map();
  void Unmap();

  // Swaps the guts of this buffer with another.
  void Swap(Buffer* other);

 private:
  int size_;
  scoped_array<unsigned char> mem_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_BUFFER_H_

