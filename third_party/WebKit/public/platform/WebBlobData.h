/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebBlobData_h
#define WebBlobData_h

#include "WebString.h"
#include "WebThreadSafeData.h"
#include "WebURL.h"

#if INSIDE_BLINK
#include <memory>
#endif

namespace blink {

class BlobData;

class WebBlobData {
 public:
  struct Item {
    enum { kTypeData, kTypeFile, kTypeBlob, kTypeFileSystemURL } type;
    WebThreadSafeData data;
    WebString blob_uuid;
    WebString file_path;
    WebURL file_system_url;
    long long offset;
    long long length;  // -1 means go to the end of the file/blob.
    double expected_modification_time;  // 0.0 means that the time is not set.
  };

  BLINK_PLATFORM_EXPORT WebBlobData();
  BLINK_PLATFORM_EXPORT ~WebBlobData();
  WebBlobData(const WebBlobData&) = delete;
  WebBlobData& operator=(const WebBlobData&) = delete;

  bool IsNull() const { return !private_.get(); }

  // Returns the number of items.
  BLINK_PLATFORM_EXPORT size_t ItemCount() const;

  // Retrieves the values of the item at the given index. Returns false if
  // index is out of bounds.
  // This call is single use only per index. The memory ownership is
  // transfered to the result in the case of memory items. A second call with
  // the same index will result in null data.
  // TODO(dmurph): change the name to 'takeItemAt'
  BLINK_PLATFORM_EXPORT bool ItemAt(size_t index, Item& result) const;

  BLINK_PLATFORM_EXPORT WebString ContentType() const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebBlobData(std::unique_ptr<BlobData>);
  BLINK_PLATFORM_EXPORT WebBlobData& operator=(std::unique_ptr<BlobData>);
  BLINK_PLATFORM_EXPORT operator std::unique_ptr<BlobData>();
#endif

 private:
  std::unique_ptr<BlobData> private_;
};

}  // namespace blink

#endif  // WebBlobData_h
