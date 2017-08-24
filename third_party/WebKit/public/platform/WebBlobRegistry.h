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

#ifndef WebBlobRegistry_h
#define WebBlobRegistry_h

#include "WebCommon.h"
#include "WebThreadSafeData.h"
#include "mojo/public/cpp/system/message_pipe.h"

#include <memory>

namespace blink {

class WebBlobData;
class WebString;
class WebURL;

// Acts as singleton facade for all Blob interactions ouside of blink.  This
// includes blob:
// * creation,
// * reference counting,
// * publishing, and
// * streaming.
class WebBlobRegistry {
 public:
  // Builder class for creating blobs. The blob is built on calling the
  // Build() method, where IPCs are sent to the browser.
  // Preconditions:
  // * Not meant to be used on multiple threads.
  // * Must not be kept alive longer than creator WebBlobRegistry (shouldn't
  //   be an issue because of the singleton nature of the WebBlobRegistry)
  // * Append.* methods are invalid after Build() is called.
  class Builder {
   public:
    virtual ~Builder() {}
    virtual void AppendData(const WebThreadSafeData&) = 0;
    virtual void AppendFile(const WebString& path,
                            uint64_t offset,
                            uint64_t length,
                            double expected_modification_time) = 0;
    // Calling this method ensures the given blob lives for the creation of
    // the new blob.
    virtual void AppendBlob(const WebString& uuid,
                            uint64_t offset,
                            uint64_t length) = 0;
    virtual void AppendFileSystemURL(const WebURL&,
                                     uint64_t offset,
                                     uint64_t length,
                                     double expected_modification_time) = 0;

    // Builds the blob. All calls to Append* are invalid after calling this
    // method.
    virtual void Build() = 0;
  };

  virtual ~WebBlobRegistry() {}

  // TODO(dmurph): Deprecate and migrate to CreateBuilder
  virtual void RegisterBlobData(const WebString& uuid, const WebBlobData&) {}

  // The blob is finalized (and sent to the browser) on calling Build() on the
  // Builder object.
  virtual std::unique_ptr<Builder> CreateBuilder(
      const WebString& uuid,
      const WebString& content_type) = 0;

  virtual void AddBlobDataRef(const WebString& uuid) {}
  virtual void RemoveBlobDataRef(const WebString& uuid) {}
  virtual void RegisterPublicBlobURL(const WebURL&, const WebString& uuid) {}
  virtual void RevokePublicBlobURL(const WebURL&) {}
};

}  // namespace blink

#endif  // WebBlobRegistry_h
