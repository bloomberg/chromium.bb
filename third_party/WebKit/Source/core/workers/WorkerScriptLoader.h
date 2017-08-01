/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerScriptLoader_h
#define WorkerScriptLoader_h

#include <memory>
#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class ResourceRequest;
class ResourceResponse;
class ExecutionContext;
class TextResourceDecoder;

class CORE_EXPORT WorkerScriptLoader final
    : public RefCounted<WorkerScriptLoader>,
      public ThreadableLoaderClient {
  USING_FAST_MALLOC(WorkerScriptLoader);

 public:
  static RefPtr<WorkerScriptLoader> Create() {
    return AdoptRef(new WorkerScriptLoader());
  }

  void LoadSynchronously(ExecutionContext&,
                         const KURL&,
                         WebURLRequest::RequestContext,
                         WebAddressSpace);

  // Note that callbacks could be invoked before loadAsynchronously() returns.
  void LoadAsynchronously(ExecutionContext&,
                          const KURL&,
                          WebURLRequest::RequestContext,
                          WebURLRequest::FetchRequestMode,
                          WebURLRequest::FetchCredentialsMode,
                          WebAddressSpace,
                          WTF::Closure response_callback,
                          WTF::Closure finished_callback);

  // This will immediately invoke |finishedCallback| if loadAsynchronously()
  // is in progress.
  void Cancel();

  String SourceText();
  const KURL& Url() const { return url_; }
  const KURL& ResponseURL() const;
  bool Failed() const { return failed_; }
  bool Canceled() const { return canceled_; }
  unsigned long Identifier() const { return identifier_; }
  long long AppCacheID() const { return app_cache_id_; }

  std::unique_ptr<Vector<char>> ReleaseCachedMetadata() {
    return std::move(cached_metadata_);
  }
  const Vector<char>* CachedMetadata() const { return cached_metadata_.get(); }

  ContentSecurityPolicy* GetContentSecurityPolicy() {
    return content_security_policy_.Get();
  }
  ContentSecurityPolicy* ReleaseContentSecurityPolicy() {
    return content_security_policy_.Release();
  }

  String GetReferrerPolicy() { return referrer_policy_; }

  WebAddressSpace ResponseAddressSpace() const {
    return response_address_space_;
  }

  const Vector<String>* OriginTrialTokens() const {
    return origin_trial_tokens_.get();
  }

  // ThreadableLoaderClient
  void DidReceiveResponse(unsigned long /*identifier*/,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveData(const char* data, unsigned data_length) override;
  void DidReceiveCachedMetadata(const char*, int /*dataLength*/) override;
  void DidFinishLoading(unsigned long identifier, double) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

 private:
  friend class WTF::RefCounted<WorkerScriptLoader>;

  WorkerScriptLoader();
  ~WorkerScriptLoader() override;

  void NotifyError();
  void NotifyFinished();

  void ProcessContentSecurityPolicy(const ResourceResponse&);

  // Callbacks for loadAsynchronously().
  WTF::Closure response_callback_;
  WTF::Closure finished_callback_;

  Persistent<ThreadableLoader> threadable_loader_;
  String response_encoding_;
  std::unique_ptr<TextResourceDecoder> decoder_;
  StringBuilder source_text_;
  KURL url_;
  KURL response_url_;

  // TODO(nhiroki): Consolidate these state flags for cleanup.
  bool failed_ = false;
  bool canceled_ = false;
  bool need_to_cancel_ = false;

  unsigned long identifier_ = 0;
  long long app_cache_id_ = 0;
  std::unique_ptr<Vector<char>> cached_metadata_;
  Persistent<ContentSecurityPolicy> content_security_policy_;
  Persistent<ExecutionContext> execution_context_;
  WebAddressSpace response_address_space_;
  std::unique_ptr<Vector<String>> origin_trial_tokens_;
  String referrer_policy_;
};

}  // namespace blink

#endif  // WorkerScriptLoader_h
