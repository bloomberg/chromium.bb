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

#include "public/web/WebHistoryItem.h"

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/loader/HistoryItem.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/StringHash.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebHTTPBody.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/web/WebSerializedScriptValue.h"

namespace blink {

void WebHistoryItem::Initialize() {
  private_ = HistoryItem::Create();
}

void WebHistoryItem::Reset() {
  private_.Reset();
  target_.Reset();
}

void WebHistoryItem::Assign(const WebHistoryItem& other) {
  private_ = other.private_;
  target_ = other.target_;
}

WebString WebHistoryItem::UrlString() const {
  return private_->UrlString();
}

void WebHistoryItem::SetURLString(const WebString& url) {
  private_->SetURLString(KURL(kParsedURLString, url).GetString());
}

WebString WebHistoryItem::GetReferrer() const {
  return private_->GetReferrer().referrer;
}

WebReferrerPolicy WebHistoryItem::GetReferrerPolicy() const {
  return static_cast<WebReferrerPolicy>(
      private_->GetReferrer().referrer_policy);
}

void WebHistoryItem::SetReferrer(const WebString& referrer,
                                 WebReferrerPolicy referrer_policy) {
  private_->SetReferrer(
      Referrer(referrer, static_cast<ReferrerPolicy>(referrer_policy)));
}

const WebString& WebHistoryItem::Target() const {
  return target_;
}

void WebHistoryItem::SetTarget(const WebString& target) {
  target_ = target;
}

WebFloatPoint WebHistoryItem::VisualViewportScrollOffset() const {
  HistoryItem::ViewState* scroll_and_view_state = private_->GetViewState();
  ScrollOffset offset =
      scroll_and_view_state
          ? scroll_and_view_state->visual_viewport_scroll_offset_
          : ScrollOffset();
  return WebFloatPoint(offset.Width(), offset.Height());
}

void WebHistoryItem::SetVisualViewportScrollOffset(
    const WebFloatPoint& scroll_offset) {
  private_->SetVisualViewportScrollOffset(ToScrollOffset(scroll_offset));
}

WebPoint WebHistoryItem::GetScrollOffset() const {
  HistoryItem::ViewState* scroll_and_view_state = private_->GetViewState();
  ScrollOffset offset = scroll_and_view_state
                            ? scroll_and_view_state->scroll_offset_
                            : ScrollOffset();
  return WebPoint(offset.Width(), offset.Height());
}

void WebHistoryItem::SetScrollOffset(const WebPoint& scroll_offset) {
  private_->SetScrollOffset(ScrollOffset(scroll_offset.x, scroll_offset.y));
}

float WebHistoryItem::PageScaleFactor() const {
  HistoryItem::ViewState* scroll_and_view_state = private_->GetViewState();
  return scroll_and_view_state ? scroll_and_view_state->page_scale_factor_ : 0;
}

void WebHistoryItem::SetPageScaleFactor(float scale) {
  private_->SetPageScaleFactor(scale);
}

WebVector<WebString> WebHistoryItem::GetDocumentState() const {
  return private_->GetDocumentState();
}

void WebHistoryItem::SetDocumentState(const WebVector<WebString>& state) {
  // FIXME: would be nice to avoid the intermediate copy
  Vector<String> ds;
  for (size_t i = 0; i < state.size(); ++i)
    ds.push_back(state[i]);
  private_->SetDocumentState(ds);
}

long long WebHistoryItem::ItemSequenceNumber() const {
  return private_->ItemSequenceNumber();
}

void WebHistoryItem::SetItemSequenceNumber(long long item_sequence_number) {
  private_->SetItemSequenceNumber(item_sequence_number);
}

long long WebHistoryItem::DocumentSequenceNumber() const {
  return private_->DocumentSequenceNumber();
}

void WebHistoryItem::SetDocumentSequenceNumber(
    long long document_sequence_number) {
  private_->SetDocumentSequenceNumber(document_sequence_number);
}

WebHistoryScrollRestorationType WebHistoryItem::ScrollRestorationType() const {
  return static_cast<WebHistoryScrollRestorationType>(
      private_->ScrollRestorationType());
}

void WebHistoryItem::SetScrollRestorationType(
    WebHistoryScrollRestorationType type) {
  private_->SetScrollRestorationType(
      static_cast<HistoryScrollRestorationType>(type));
}

WebSerializedScriptValue WebHistoryItem::StateObject() const {
  return WebSerializedScriptValue(private_->StateObject());
}

void WebHistoryItem::SetStateObject(const WebSerializedScriptValue& object) {
  private_->SetStateObject(object);
}

WebString WebHistoryItem::HttpContentType() const {
  return private_->FormContentType();
}

void WebHistoryItem::SetHTTPContentType(const WebString& http_content_type) {
  private_->SetFormContentType(http_content_type);
}

WebHTTPBody WebHistoryItem::HttpBody() const {
  return WebHTTPBody(private_->FormData());
}

void WebHistoryItem::SetHTTPBody(const WebHTTPBody& http_body) {
  private_->SetFormData(http_body);
}

WebVector<WebString> WebHistoryItem::GetReferencedFilePaths() const {
  HashSet<String> file_paths;
  const EncodedFormData* form_data = private_->FormData();
  if (form_data) {
    for (size_t i = 0; i < form_data->Elements().size(); ++i) {
      const FormDataElement& element = form_data->Elements()[i];
      if (element.type_ == FormDataElement::kEncodedFile)
        file_paths.insert(element.filename_);
    }
  }

  const Vector<String>& referenced_file_paths =
      private_->GetReferencedFilePaths();
  for (size_t i = 0; i < referenced_file_paths.size(); ++i)
    file_paths.insert(referenced_file_paths[i]);

  Vector<String> results;
  CopyToVector(file_paths, results);
  return results;
}

bool WebHistoryItem::DidSaveScrollOrScaleState() const {
  return private_->GetViewState();
}

WebHistoryItem::WebHistoryItem(HistoryItem* item) : private_(item) {}

WebHistoryItem& WebHistoryItem::operator=(HistoryItem* item) {
  private_ = item;
  return *this;
}

WebHistoryItem::operator HistoryItem*() const {
  return private_.Get();
}

}  // namespace blink
