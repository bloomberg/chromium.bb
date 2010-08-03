/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains implementation for raw-data which is used
// by the progressive streaming archive system
//
// A RawData object is just a pointer to some memory of a given length.
// This data may represent string data, image data, audio data, etc..

#ifndef O3D_IMPORT_CROSS_RAW_DATA_H_
#define O3D_IMPORT_CROSS_RAW_DATA_H_

#include "base/scoped_ptr.h"
#include "core/cross/param_object.h"

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class RawData : public ParamObject {
 public:
  typedef SmartPointer<RawData> Ref;

  static RawData::Ref Create(ServiceLocator* service_locator,
                             const String &uri,
                             const void *data,
                             size_t length);

  static RawData::Ref CreateFromFile(ServiceLocator* service_locator,
                                     const String &uri,
                                     const String& filename);

  // Creates a RawData object, taking as input a string containing a
  // data URL.
  static RawData::Ref CreateFromDataURL(ServiceLocator* service_locator,
                                        const String& data_url);

  virtual ~RawData();

  const uint8 *GetData() const;

  template <typename T>
  const T* GetDataAs(size_t offset) const {
    return reinterpret_cast<const T*>(GetData() + offset);
  };

  size_t GetLength() const { return length_; }

  String StringValue() const;

  const String& uri() const { return uri_; }
  void set_uri(const String& uri) { uri_ = uri; }

  // Historically this wrote the data out to a temp file and deleted it from
  // memory, but that functionality was removed due to security concerns. In any
  // event, a RawData object that is big enough to be worth removing from memory
  // will occupy multiple complete pages which won't be in the process's working
  // set, so the OS will eventually remove it from the physical memory anyway
  // and bring it back in when we next access it.
  void Flush() {}

  // deletes the data
  void Discard();

  bool IsOffsetLengthValid(size_t offset, size_t length) const;

 private:
  String uri_;
  scoped_array<uint8> data_;
  size_t length_;
  bool allow_string_value_;

  RawData(ServiceLocator* service_locator,
          const String &uri,
          const void *data,
          size_t length);

  bool SetFromFile(const String& filename);

  // Decodes data from a data URL and stores that data in this
  // RawData object. Returns false on error, true otherwise
  bool SetFromDataURL(const String& data_url);

  friend class IClassManager;
  friend class Pack;

  O3D_DECL_CLASS(RawData, ParamObject)
  DISALLOW_COPY_AND_ASSIGN(RawData);
};

}  // namespace o3d

#endif  // O3D_IMPORT_CROSS_RAW_DATA_H_
