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

#include "core/frame/WebLocalFrameImpl.h"

#include "core/editing/finder/TextFinder.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFindOptions.h"
#include "public/web/WebFrameClient.h"

namespace blink {

void WebLocalFrameImpl::RequestFind(int identifier,
                                    const WebString& search_text,
                                    const WebFindOptions& options) {
  // Send "no results" if this frame has no visible content.
  if (!HasVisibleContent() && !options.force) {
    Client()->ReportFindInPageMatchCount(identifier, 0 /* count */,
                                         true /* finalUpdate */);
    return;
  }

  WebRange current_selection = SelectionRange();
  bool result = false;
  bool active_now = false;

  // Search for an active match only if this frame is focused or if this is a
  // find next request.
  if (IsFocused() || options.find_next) {
    result = Find(identifier, search_text, options, false /* wrapWithinFrame */,
                  &active_now);
  }

  if (result && !options.find_next) {
    // Indicate that at least one match has been found. 1 here means
    // possibly more matches could be coming.
    Client()->ReportFindInPageMatchCount(identifier, 1 /* count */,
                                         false /* finalUpdate */);
  }

  // There are three cases in which scoping is needed:
  //
  // (1) This is an initial find request (|options.findNext| is false). This
  // will be the first scoping effort for this find session.
  //
  // (2) Something has been selected since the last search. This means that we
  // cannot just increment the current match ordinal; we need to re-generate
  // it.
  //
  // (3) TextFinder::Find() found what should be the next match (|result| is
  // true), but was unable to activate it (|activeNow| is false). This means
  // that the text containing this match was dynamically added since the last
  // scope of the frame. The frame needs to be re-scoped so that any matches
  // in the new text can be highlighted and included in the reported number of
  // matches.
  //
  // If none of these cases are true, then we just report the current match
  // count without scoping.
  if (/* (1) */ options.find_next && /* (2) */ current_selection.IsNull() &&
      /* (3) */ !(result && !active_now)) {
    // Force report of the actual count.
    IncreaseMatchCount(0, identifier);
    return;
  }

  // Start a new scoping request. If the scoping function determines that it
  // needs to scope, it will defer until later.
  EnsureTextFinder().StartScopingStringMatches(identifier, search_text,
                                               options);
}

bool WebLocalFrameImpl::Find(int identifier,
                             const WebString& search_text,
                             const WebFindOptions& options,
                             bool wrap_within_frame,
                             bool* active_now) {
  if (!GetFrame())
    return false;

  // Unlikely, but just in case we try to find-in-page on a detached frame.
  DCHECK(GetFrame()->GetPage());

  // Up-to-date, clean tree is required for finding text in page, since it
  // relies on TextIterator to look over the text.
  GetFrame()->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  return EnsureTextFinder().Find(identifier, search_text, options,
                                 wrap_within_frame, active_now);
}

void WebLocalFrameImpl::StopFinding(StopFindAction action) {
  bool clear_selection = action == kStopFindActionClearSelection;
  if (clear_selection)
    ExecuteCommand(WebString::FromUTF8("Unselect"));

  if (text_finder_) {
    if (!clear_selection)
      text_finder_->SetFindEndstateFocusAndSelection();
    text_finder_->StopFindingAndClearSelection();
  }

  if (action == kStopFindActionActivateSelection && IsFocused()) {
    WebDocument doc = GetDocument();
    if (!doc.IsNull()) {
      WebElement element = doc.FocusedElement();
      if (!element.IsNull())
        element.SimulateClick();
    }
  }
}

void WebLocalFrameImpl::IncreaseMatchCount(int count, int identifier) {
  EnsureTextFinder().IncreaseMatchCount(identifier, count);
}

int WebLocalFrameImpl::SelectNearestFindMatch(const WebFloatPoint& point,
                                              WebRect* selection_rect) {
  return EnsureTextFinder().SelectNearestFindMatch(point, selection_rect);
}

int WebLocalFrameImpl::FindMatchMarkersVersion() const {
  if (text_finder_)
    return text_finder_->FindMatchMarkersVersion();
  return 0;
}

float WebLocalFrameImpl::DistanceToNearestFindMatch(
    const WebFloatPoint& point) {
  float nearest_distance;
  EnsureTextFinder().NearestFindMatch(point, &nearest_distance);
  return nearest_distance;
}

WebFloatRect WebLocalFrameImpl::ActiveFindMatchRect() {
  if (text_finder_)
    return text_finder_->ActiveFindMatchRect();
  return WebFloatRect();
}

void WebLocalFrameImpl::FindMatchRects(WebVector<WebFloatRect>& output_rects) {
  EnsureTextFinder().FindMatchRects(output_rects);
}

void WebLocalFrameImpl::SetTickmarks(const WebVector<WebRect>& tickmarks) {
  if (GetFrameView()) {
    Vector<IntRect> tickmarks_converted(tickmarks.size());
    for (size_t i = 0; i < tickmarks.size(); ++i)
      tickmarks_converted[i] = tickmarks[i];
    GetFrameView()->SetTickmarks(tickmarks_converted);
  }
}

TextFinder* WebLocalFrameImpl::GetTextFinder() const {
  return text_finder_;
}

TextFinder& WebLocalFrameImpl::EnsureTextFinder() {
  if (!text_finder_)
    text_finder_ = TextFinder::Create(*this);

  return *text_finder_;
}

}  // namespace blink
