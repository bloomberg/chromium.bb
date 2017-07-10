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

#include "core/exported/WebDataSourceImpl.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/SubresourceFilter.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"

namespace blink {

WebDataSourceImpl* WebDataSourceImpl::Create(
    LocalFrame* frame,
    const ResourceRequest& request,
    const SubstituteData& data,
    ClientRedirectPolicy client_redirect_policy) {
  DCHECK(frame);

  return new WebDataSourceImpl(frame, request, data, client_redirect_policy);
}

const WebURLRequest& WebDataSourceImpl::OriginalRequest() const {
  return original_request_wrapper_;
}

const WebURLRequest& WebDataSourceImpl::GetRequest() const {
  return request_wrapper_;
}

const WebURLResponse& WebDataSourceImpl::GetResponse() const {
  return response_wrapper_;
}

bool WebDataSourceImpl::HasUnreachableURL() const {
  return !DocumentLoader::UnreachableURL().IsEmpty();
}

WebURL WebDataSourceImpl::UnreachableURL() const {
  return DocumentLoader::UnreachableURL();
}

void WebDataSourceImpl::AppendRedirect(const WebURL& url) {
  DocumentLoader::AppendRedirect(url);
}

void WebDataSourceImpl::UpdateNavigation(double redirect_start_time,
                                         double redirect_end_time,
                                         double fetch_start_time,
                                         bool has_redirect) {
  // Updates the redirection timing if there is at least one redirection
  // (between two URLs).
  if (has_redirect) {
    GetTiming().SetRedirectStart(redirect_start_time);
    GetTiming().SetRedirectEnd(redirect_end_time);
  }
  GetTiming().SetFetchStart(fetch_start_time);
}

void WebDataSourceImpl::RedirectChain(WebVector<WebURL>& result) const {
  result.Assign(redirect_chain_);
}

bool WebDataSourceImpl::IsClientRedirect() const {
  return DocumentLoader::IsClientRedirect();
}

bool WebDataSourceImpl::ReplacesCurrentHistoryItem() const {
  return DocumentLoader::ReplacesCurrentHistoryItem();
}

WebNavigationType WebDataSourceImpl::GetNavigationType() const {
  return ToWebNavigationType(DocumentLoader::GetNavigationType());
}

WebDataSource::ExtraData* WebDataSourceImpl::GetExtraData() const {
  return extra_data_.get();
}

void WebDataSourceImpl::SetExtraData(ExtraData* extra_data) {
  // extraData can't be a std::unique_ptr because setExtraData is a WebKit API
  // function.
  extra_data_ = WTF::WrapUnique(extra_data);
}

void WebDataSourceImpl::SetNavigationStartTime(double navigation_start) {
  GetTiming().SetNavigationStart(navigation_start);
}

WebNavigationType WebDataSourceImpl::ToWebNavigationType(NavigationType type) {
  switch (type) {
    case kNavigationTypeLinkClicked:
      return kWebNavigationTypeLinkClicked;
    case kNavigationTypeFormSubmitted:
      return kWebNavigationTypeFormSubmitted;
    case kNavigationTypeBackForward:
      return kWebNavigationTypeBackForward;
    case kNavigationTypeReload:
      return kWebNavigationTypeReload;
    case kNavigationTypeFormResubmitted:
      return kWebNavigationTypeFormResubmitted;
    case kNavigationTypeOther:
    default:
      return kWebNavigationTypeOther;
  }
}

WebDataSourceImpl::WebDataSourceImpl(
    LocalFrame* frame,
    const ResourceRequest& request,
    const SubstituteData& data,
    ClientRedirectPolicy client_redirect_policy)
    : DocumentLoader(frame, request, data, client_redirect_policy),
      original_request_wrapper_(DocumentLoader::OriginalRequest()),
      request_wrapper_(DocumentLoader::GetRequest()),
      response_wrapper_(DocumentLoader::GetResponse()) {}

WebDataSourceImpl::~WebDataSourceImpl() {
  // Verify that detachFromFrame() has been called.
  DCHECK(!extra_data_);
}

void WebDataSourceImpl::DetachFromFrame() {
  DocumentLoader::DetachFromFrame();
  extra_data_.reset();
}

void WebDataSourceImpl::SetSubresourceFilter(
    WebDocumentSubresourceFilter* subresource_filter) {
  DocumentLoader::SetSubresourceFilter(SubresourceFilter::Create(
      *GetFrame()->GetDocument(), WTF::WrapUnique(subresource_filter)));
}

void WebDataSourceImpl::SetServiceWorkerNetworkProvider(
    std::unique_ptr<WebServiceWorkerNetworkProvider> provider) {
  DocumentLoader::SetServiceWorkerNetworkProvider(std::move(provider));
}

WebServiceWorkerNetworkProvider*
WebDataSourceImpl::GetServiceWorkerNetworkProvider() {
  return DocumentLoader::GetServiceWorkerNetworkProvider();
}

void WebDataSourceImpl::SetSourceLocation(
    const WebSourceLocation& source_location) {
  std::unique_ptr<SourceLocation> location =
      SourceLocation::Create(source_location.url, source_location.line_number,
                             source_location.column_number, nullptr);
  DocumentLoader::SetSourceLocation(std::move(location));
}

void WebDataSourceImpl::ResetSourceLocation() {
  DocumentLoader::SetSourceLocation(nullptr);
}

DEFINE_TRACE(WebDataSourceImpl) {
  DocumentLoader::Trace(visitor);
}

}  // namespace blink
