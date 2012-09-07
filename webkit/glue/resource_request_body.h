// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_RESOURCE_REQUEST_BODY_H_
#define WEBKIT_GLUE_RESOURCE_REQUEST_BODY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "webkit/base/data_element.h"
#include "webkit/glue/webkit_glue_export.h"

namespace net {
class UploadData;
class UploadElement;
}

namespace webkit_blob {
class BlobStorageController;
}

namespace webkit_glue {

// A struct used to represent upload data. The data field is populated by
// WebURLLoader from the data given as WebHTTPBody.
class WEBKIT_GLUE_EXPORT ResourceRequestBody
    : public base::RefCounted<ResourceRequestBody>,
      public base::SupportsUserData {
 public:
  typedef webkit_base::DataElement Element;

  ResourceRequestBody();

  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const FilePath& file_path,
                       uint64 offset, uint64 length,
                       const base::Time& expected_modification_time);
  void AppendBlob(const GURL& blob_url);
  void AppendFileSystemFileRange(const GURL& url, uint64 offset, uint64 length,
                                 const base::Time& expected_modification_time);

  // Creates a new UploadData from this request body. This also resolves
  // any blob references using given |blob_controller|.
  // TODO(kinuko): Clean up this hack.
  net::UploadData* ResolveElementsAndCreateUploadData(
      webkit_blob::BlobStorageController* blob_controller);

  const std::vector<Element>* elements() const { return &elements_; }
  std::vector<Element>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64 id) { identifier_ = id; }
  int64 identifier() const { return identifier_; }

 private:
  friend class base::RefCounted<ResourceRequestBody>;
  virtual ~ResourceRequestBody();

  // Resolves the |blob_url| using |blob_controller| and appends resolved
  // items to |resolved_elements|.
  void ResolveBlobReference(webkit_blob::BlobStorageController* blob_controller,
                            const GURL& blob_url,
                            std::vector<const Element*>* resolved_elements);

  std::vector<Element> elements_;
  int64 identifier_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_RESOURCE_REQUEST_BODY_H_
