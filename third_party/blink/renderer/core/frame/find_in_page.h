// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FIND_IN_PAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FIND_IN_PAGE_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {

class TextFinder;
class WebLocalFrameImpl;
class WebString;
struct WebFindOptions;
struct WebFloatRect;

class FindInPage : public GarbageCollected<FindInPage> {
 public:
  static FindInPage* Create(WebLocalFrameImpl& frame) {
    return new FindInPage(frame);
  }

  void RequestFind(int identifier,
                   const WebString& search_text,
                   const WebFindOptions&);

  bool Find(int identifier,
            const WebString& search_text,
            const WebFindOptions&,
            bool wrap_within_frame,
            bool* active_now = nullptr);

  void StopFinding(WebLocalFrame::StopFindAction);

  void IncreaseMatchCount(int count, int identifier);

  int FindMatchMarkersVersion() const;

  WebFloatRect ActiveFindMatchRect();

  void FindMatchRects(WebVector<WebFloatRect>&);

  int SelectNearestFindMatch(const WebFloatPoint&, WebRect* selection_rect);

  float DistanceToNearestFindMatch(const WebFloatPoint&);

  void SetTickmarks(const WebVector<WebRect>&);

  void ClearActiveFindMatch();

  TextFinder* GetTextFinder() const;

  // Returns the text finder object if it already exists.
  // Otherwise creates it and then returns.
  TextFinder& EnsureTextFinder();

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(text_finder_);
    visitor->Trace(frame_);
  }

 private:
  FindInPage(WebLocalFrameImpl& frame) : frame_(&frame) {}

  // Will be initialized after first call to ensureTextFinder().
  Member<TextFinder> text_finder_;

  const Member<WebLocalFrameImpl> frame_;

  DISALLOW_COPY_AND_ASSIGN(FindInPage);
};

}  // namespace blink

#endif
