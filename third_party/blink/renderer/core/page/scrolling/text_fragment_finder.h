// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_FINDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_FINDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;

// This is a helper class to TextFragmentAnchor. It's responsible for actually
// performing the text search for the anchor and returning the results to the
// anchor.
class CORE_EXPORT TextFragmentFinder final {
 public:
  class Client {
   public:
    virtual void DidFindMatch(const EphemeralRangeInFlatTree& range) = 0;
  };

  // Client must outlive the finder.
  TextFragmentFinder(Client& client);
  ~TextFragmentFinder() = default;

  // Begins searching for the given string in the given top-level document.
  void FindMatch(const String& search_text, Document& document);

 private:
  Client& client_;

  String search_text_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentFinder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_FINDER_H_
