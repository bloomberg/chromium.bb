// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionSample_h
#define SelectionSample_h

#include <string>

#include "core/editing/Forward.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ContainerNode;
class HTMLElement;

// |SelectionSample| provides parsing HTML text with selection markers and
// serializes DOM tree with selection markers.
// Selection markers are represents by "^" for selection base and "|" for
// selection extent like "assert_selection.js" in layout test.
class SelectionSample final {
  STATIC_ONLY(SelectionSample);

 public:
  // TDOO(editng-dev): We will have flat tree version of |SetSelectionText()|
  // and |GetSelectionText()| when we need.
  // Set |selection_text|, which is HTML markup with selection markers as inner
  // HTML to |HTMLElement| and returns |SelectionInDOMTree|.
  static SelectionInDOMTree SetSelectionText(HTMLElement*,
                                             const std::string& selection_text);

  // Note: We don't add namespace declaration if |ContainerNode| doesn't
  // have it.
  // Note: We don't escape "--" in comment.
  static std::string GetSelectionText(const ContainerNode&,
                                      const SelectionInDOMTree&);
  static std::string GetSelectionTextInFlatTree(const ContainerNode&,
                                                const SelectionInFlatTree&);
  static void ConvertTemplatesToShadowRootsForTesring(HTMLElement&);
};

}  // namespace blink

#endif  // SelectionSample_h
