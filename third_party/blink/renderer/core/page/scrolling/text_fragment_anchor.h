// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalFrame;
class KURL;

class CORE_EXPORT TextFragmentAnchor final : public FragmentAnchor,
                                             public TextFragmentFinder::Client {
 public:
  static TextFragmentAnchor* TryCreate(const KURL& url, LocalFrame& frame);

  TextFragmentAnchor(
      const std::vector<TextFragmentSelector>& text_fragment_selectors,
      LocalFrame& frame);
  ~TextFragmentAnchor() override = default;

  bool Invoke() override;

  void Installed() override;

  void DidScroll(ScrollType type) override;

  void PerformPreRafActions() override;

  void DidCompleteLoad() override;

  void Trace(blink::Visitor*) override;

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& range) override;

 private:
  std::vector<TextFragmentFinder> text_fragment_finders_;

  Member<LocalFrame> frame_;

  bool search_finished_ = false;
  bool user_scrolled_ = false;
  bool first_match_needs_scroll_ = true;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentAnchor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_H_
