/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebHTTPBody_h
#define WebHTTPBody_h

#include "WebBlobData.h"
#include "WebData.h"
#include "WebNonCopyable.h"
#include "WebString.h"
#include "WebURL.h"

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#endif

namespace blink {

class EncodedFormData;

class WebHTTPBody {
 public:
  struct Element {
    enum Type { kTypeData, kTypeFile, kTypeBlob, kTypeFileSystemURL } type;
    WebData data;
    WebString file_path;
    long long file_start;
    long long file_length;  // -1 means to the end of the file.
    double modification_time;
    WebURL file_system_url;
    WebString blob_uuid;
  };

  ~WebHTTPBody() { Reset(); }

  WebHTTPBody() {}
  WebHTTPBody(const WebHTTPBody& b) { Assign(b); }
  WebHTTPBody& operator=(const WebHTTPBody& b) {
    Assign(b);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Initialize();
  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebHTTPBody&);

  bool IsNull() const { return !private_; }

  // Returns the number of elements comprising the http body.
  BLINK_PLATFORM_EXPORT size_t ElementCount() const;

  // Sets the values of the element at the given index. Returns false if
  // index is out of bounds.
  BLINK_PLATFORM_EXPORT bool ElementAt(size_t index, Element&) const;

  // Append to the list of elements.
  BLINK_PLATFORM_EXPORT void AppendData(const WebData&);
  BLINK_PLATFORM_EXPORT void AppendFile(const WebString&);
  // Passing -1 to |file_length| means to the end of the file.
  BLINK_PLATFORM_EXPORT void AppendFileRange(const WebString&,
                                             long long file_start,
                                             long long file_length,
                                             double modification_time);
  BLINK_PLATFORM_EXPORT void AppendBlob(const WebString& uuid);

  BLINK_PLATFORM_EXPORT void SetUniqueBoundary();

  // Identifies a particular form submission instance. A value of 0 is
  // used to indicate an unspecified identifier.
  BLINK_PLATFORM_EXPORT long long Identifier() const;
  BLINK_PLATFORM_EXPORT void SetIdentifier(long long);

  BLINK_PLATFORM_EXPORT bool ContainsPasswordData() const;
  BLINK_PLATFORM_EXPORT void SetContainsPasswordData(bool);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebHTTPBody(scoped_refptr<EncodedFormData>);
  BLINK_PLATFORM_EXPORT WebHTTPBody& operator=(scoped_refptr<EncodedFormData>);
  BLINK_PLATFORM_EXPORT operator scoped_refptr<EncodedFormData>() const;
#endif

 private:
  BLINK_PLATFORM_EXPORT void EnsureMutable();

  WebPrivatePtr<EncodedFormData> private_;
};

}  // namespace blink

#endif
