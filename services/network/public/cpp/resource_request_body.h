// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "services/network/public/cpp/data_element.h"
#include "url/gurl.h"

namespace network {

// ResourceRequestBody represents body (i.e. upload data) of a HTTP request.
class ResourceRequestBody
    : public base::RefCountedThreadSafe<ResourceRequestBody> {
 public:
  ResourceRequestBody();

  // Creates ResourceRequestBody that holds a copy of |bytes|.
  static scoped_refptr<ResourceRequestBody> CreateFromBytes(const char* bytes,
                                                            size_t length);

  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const base::FilePath& file_path,
                       uint64_t offset,
                       uint64_t length,
                       const base::Time& expected_modification_time);
  // Appends the specified part of |file|. If |length| extends beyond the end of
  // the file, it will be set to the end of the file.
  void AppendRawFileRange(base::File file,
                          const base::FilePath& file_path,
                          uint64_t offset,
                          uint64_t length,
                          const base::Time& expected_modification_time);

  void AppendBlob(const std::string& uuid);
  void AppendFileSystemFileRange(const GURL& url,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time);
  void AppendDataPipe(mojom::DataPipeGetterPtr data_pipe_getter);

  const std::vector<DataElement>* elements() const { return &elements_; }
  std::vector<DataElement>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<DataElement>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64_t id) { identifier_ = id; }
  int64_t identifier() const { return identifier_; }

  // Returns paths referred to by |elements| of type DataElement::TYPE_FILE.
  std::vector<base::FilePath> GetReferencedFiles() const;

  // Sets the flag which indicates whether the post data contains sensitive
  // information like passwords.
  void set_contains_sensitive_info(bool contains_sensitive_info) {
    contains_sensitive_info_ = contains_sensitive_info;
  }
  bool contains_sensitive_info() const { return contains_sensitive_info_; }

 private:
  friend class base::RefCountedThreadSafe<ResourceRequestBody>;

  ~ResourceRequestBody();

  std::vector<DataElement> elements_;
  int64_t identifier_;

  bool contains_sensitive_info_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_
