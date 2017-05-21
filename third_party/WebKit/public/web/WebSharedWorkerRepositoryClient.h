/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebSharedWorkerRepositoryClient_h
#define WebSharedWorkerRepositoryClient_h

#include "WebSharedWorkerCreationContextType.h"
#include "WebSharedWorkerCreationErrors.h"
#include "public/platform/WebAddressSpace.h"
#include <memory>

namespace blink {

class WebMessagePortChannel;
class WebSharedWorkerConnectListener;
class WebString;
class WebURL;

class WebSharedWorkerRepositoryClient {
 public:
  // Unique identifier for the parent document of a worker (unique within a
  // given process).
  using DocumentID = unsigned long long;

  // Connects to a shared worker.
  virtual void Connect(const WebURL& url,
                       const WebString& name,
                       DocumentID id,
                       const WebString& content_security_policy,
                       WebContentSecurityPolicyType,
                       WebAddressSpace,
                       WebSharedWorkerCreationContextType,
                       bool data_saver_enabled,
                       std::unique_ptr<WebMessagePortChannel>,
                       std::unique_ptr<blink::WebSharedWorkerConnectListener>) {
  }

  // Invoked when a document has been detached. DocumentID can be re-used after
  // documentDetached() is invoked.
  virtual void DocumentDetached(DocumentID) {}
};

}  // namespace blink

#endif  // WebSharedWorkerRepositoryClient_h
