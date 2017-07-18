/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef ThreadableLoaderClient_h
#define ThreadableLoaderClient_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebDataConsumerHandle.h"

namespace blink {

class KURL;
class ResourceError;
class ResourceResponse;
class ResourceTimingInfo;

class CORE_EXPORT ThreadableLoaderClient {
  WTF_MAKE_NONCOPYABLE(ThreadableLoaderClient);

 public:
  virtual void DidSendData(unsigned long long /*bytesSent*/,
                           unsigned long long /*totalBytesToBeSent*/) {}
  virtual void DidReceiveRedirectTo(const KURL&) {}
  virtual void DidReceiveResponse(unsigned long /*identifier*/,
                                  const ResourceResponse&,
                                  std::unique_ptr<WebDataConsumerHandle>) {}
  virtual void DidReceiveData(const char*, unsigned /*dataLength*/) {}
  virtual void DidReceiveCachedMetadata(const char*, int /*dataLength*/) {}
  virtual void DidFinishLoading(unsigned long /*identifier*/,
                                double /*finishTime*/) {}
  virtual void DidFail(const ResourceError&) {}
  virtual void DidFailRedirectCheck() {}
  virtual void DidReceiveResourceTiming(const ResourceTimingInfo&) {}

  virtual bool IsDocumentThreadableLoaderClient() { return false; }

  virtual void DidDownloadData(int /*dataLength*/) {}

  virtual ~ThreadableLoaderClient() {}

 protected:
  ThreadableLoaderClient() {}
};

}  // namespace blink

#endif  // ThreadableLoaderClient_h
