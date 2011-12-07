// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VAR_ARRAY_BUFFER_DEV_H_
#define PPAPI_CPP_DEV_VAR_ARRAY_BUFFER_DEV_H_

#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for interacting with an ArrayBuffer.

namespace pp {

/// VarArrayBuffer_Dev provides a way to interact with JavaScript ArrayBuffers,
/// which represent a contiguous sequence of bytes. Note that
/// VarArrayBuffer_Devs are not part of the embedding page's DOM, and can only
/// be shared with JavaScript via pp::Instance's PostMessage and HandleMessage
/// functions.
class VarArrayBuffer_Dev : public Var {
 public:
  /// Contruct a VarArrayBuffer_Dev given a var for which is_array_buffer() is
  /// true. This will refer to the same buffer as var, but allows you to access
  /// methods specific to VarArrayBuffer_Dev.
  ///
  /// @param[in] var An array buffer Var.
  explicit VarArrayBuffer_Dev(const Var& var);

  VarArrayBuffer_Dev(const VarArrayBuffer_Dev& buffer) : Var(buffer) {}

  /// Construct a new VarArrayBuffer_Dev which is size_in_bytes bytes long and
  /// initialized to zero.
  ///
  /// @param[in] size_in_bytes The size of the constructed array in bytes.
  VarArrayBuffer_Dev(uint32_t size_in_bytes);

  /// Return the length of the VarArrayBuffer_Dev in bytes.
  ///
  /// @return The length of the VarArrayBuffer_Dev in bytes.
  uint32_t ByteLength() const;

  /// Return a pointer to the buffer associated with this VarArrayBuffer_Dev.
  ///
  /// @return A pointer to the buffer associated with this VarArrayBuffer_Dev.
  void* Map();
  const void* Map() const;

  virtual ~VarArrayBuffer_Dev() {}

 private:
  // We cache the buffer so that repeated calls to Map() are quick.
  void* buffer_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VAR_ARRAY_BUFFER_DEV_H_
