/*
 * Copyright (C) 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
 */

#ifndef HistoryItem_h
#define HistoryItem_h

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "core/loader/FrameLoaderTypes.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntPoint.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DocumentState;
class EncodedFormData;
class KURL;
class ResourceRequest;
enum class WebCachePolicy;

class CORE_EXPORT HistoryItem final
    : public GarbageCollectedFinalized<HistoryItem> {
 public:
  static HistoryItem* Create() { return new HistoryItem; }
  ~HistoryItem();

  const String& UrlString() const;
  KURL Url() const;

  const Referrer& GetReferrer() const;

  EncodedFormData* FormData();
  const AtomicString& FormContentType() const;

  void SetDidSaveScrollOrScaleState(bool did_save_scroll_or_scale_state) {
    did_save_scroll_or_scale_state_ = did_save_scroll_or_scale_state;
  }

  bool DidSaveScrollOrScaleState() const {
    return did_save_scroll_or_scale_state_;
  }

  const ScrollOffset& VisualViewportScrollOffset() const;
  void SetVisualViewportScrollOffset(const ScrollOffset&);
  const ScrollOffset& GetScrollOffset() const;
  void SetScrollOffset(const ScrollOffset&);

  float PageScaleFactor() const;
  void SetPageScaleFactor(float);

  Vector<String> GetReferencedFilePaths();
  const Vector<String>& GetDocumentState();
  void SetDocumentState(const Vector<String>&);
  void SetDocumentState(DocumentState*);
  void ClearDocumentState();

  void SetURL(const KURL&);
  void SetURLString(const String&);
  void SetReferrer(const Referrer&);

  void SetStateObject(PassRefPtr<SerializedScriptValue>);
  SerializedScriptValue* StateObject() const { return state_object_.Get(); }

  void SetItemSequenceNumber(long long number) {
    item_sequence_number_ = number;
  }
  long long ItemSequenceNumber() const { return item_sequence_number_; }

  void SetDocumentSequenceNumber(long long number) {
    document_sequence_number_ = number;
  }
  long long DocumentSequenceNumber() const { return document_sequence_number_; }

  void SetScrollRestorationType(HistoryScrollRestorationType type) {
    scroll_restoration_type_ = type;
  }
  HistoryScrollRestorationType ScrollRestorationType() {
    return scroll_restoration_type_;
  }

  void SetFormInfoFromRequest(const ResourceRequest&);
  void SetFormData(PassRefPtr<EncodedFormData>);
  void SetFormContentType(const AtomicString&);

  ResourceRequest GenerateResourceRequest(WebCachePolicy);

  DECLARE_TRACE();

 private:
  HistoryItem();

  String url_string_;
  Referrer referrer_;

  bool did_save_scroll_or_scale_state_;
  ScrollOffset visual_viewport_scroll_offset_;
  ScrollOffset scroll_offset_;
  float page_scale_factor_;
  Vector<String> document_state_vector_;
  Member<DocumentState> document_state_;

  // If two HistoryItems have the same item sequence number, then they are
  // clones of one another. Traversing history from one such HistoryItem to
  // another is a no-op. HistoryItem clones are created for parent and
  // sibling frames when only a subframe navigates.
  int64_t item_sequence_number_;

  // If two HistoryItems have the same document sequence number, then they
  // refer to the same instance of a document. Traversing history from one
  // such HistoryItem to another preserves the document.
  int64_t document_sequence_number_;

  // Type of the scroll restoration for the history item determines if scroll
  // position should be restored when it is loaded during history traversal.
  HistoryScrollRestorationType scroll_restoration_type_;

  // Support for HTML5 History
  RefPtr<SerializedScriptValue> state_object_;

  // info used to repost form data
  RefPtr<EncodedFormData> form_data_;
  AtomicString form_content_type_;
};  // class HistoryItem

}  // namespace blink

#endif  // HISTORYITEM_H
