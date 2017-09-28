/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "public/platform/WebPrerender.h"

#include <memory>
#include "platform/Prerender.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

namespace {

class PrerenderExtraDataContainer : public Prerender::ExtraData {
 public:
  static RefPtr<PrerenderExtraDataContainer> Create(
      WebPrerender::ExtraData* extra_data) {
    return WTF::AdoptRef(new PrerenderExtraDataContainer(extra_data));
  }

  ~PrerenderExtraDataContainer() override {}

  WebPrerender::ExtraData* GetExtraData() const { return extra_data_.get(); }

 private:
  explicit PrerenderExtraDataContainer(WebPrerender::ExtraData* extra_data)
      : extra_data_(WTF::WrapUnique(extra_data)) {}

  std::unique_ptr<WebPrerender::ExtraData> extra_data_;
};

}  // namespace

WebPrerender::WebPrerender(Prerender* prerender) : private_(prerender) {}

const Prerender* WebPrerender::ToPrerender() const {
  return private_.Get();
}

void WebPrerender::Reset() {
  private_.Reset();
}

void WebPrerender::Assign(const WebPrerender& other) {
  private_ = other.private_;
}

bool WebPrerender::IsNull() const {
  return private_.IsNull();
}

WebURL WebPrerender::Url() const {
  return WebURL(private_->Url());
}

unsigned WebPrerender::RelTypes() const {
  return private_->RelTypes();
}

WebString WebPrerender::GetReferrer() const {
  return private_->GetReferrer();
}

WebReferrerPolicy WebPrerender::GetReferrerPolicy() const {
  return static_cast<WebReferrerPolicy>(private_->GetReferrerPolicy());
}

void WebPrerender::SetExtraData(WebPrerender::ExtraData* extra_data) {
  private_->SetExtraData(PrerenderExtraDataContainer::Create(extra_data));
}

const WebPrerender::ExtraData* WebPrerender::GetExtraData() const {
  RefPtr<Prerender::ExtraData> webcore_extra_data = private_->GetExtraData();
  if (!webcore_extra_data)
    return 0;
  return static_cast<PrerenderExtraDataContainer*>(webcore_extra_data.get())
      ->GetExtraData();
}

void WebPrerender::DidStartPrerender() {
  private_->DidStartPrerender();
}

void WebPrerender::DidStopPrerender() {
  private_->DidStopPrerender();
}

void WebPrerender::DidSendLoadForPrerender() {
  private_->DidSendLoadForPrerender();
}

void WebPrerender::DidSendDOMContentLoadedForPrerender() {
  private_->DidSendDOMContentLoadedForPrerender();
}

}  // namespace blink
