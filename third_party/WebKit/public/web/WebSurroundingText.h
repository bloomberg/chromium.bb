/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSurroundingText_h
#define WebSurroundingText_h

#include "public/platform/WebString.h"
#include "WebNode.h"
#include "WebRange.h"
#include <memory>

namespace blink {

class SurroundingText;
class WebNode;
class WebRange;
class WebLocalFrame;
struct WebPoint;

// WebSurroundingText is a Blink API that gives access to the SurroundingText
// API. It allows caller to know the text surrounding a point or a range.
class WebSurroundingText {
 public:
  BLINK_EXPORT WebSurroundingText();
  BLINK_EXPORT ~WebSurroundingText();

  BLINK_EXPORT bool IsNull() const;

  // Initializes the object with the current selection in a given frame.
  // The maximum length of the contents retrieved is defined by maxLength.
  // It does not include the text inside the range.
  BLINK_EXPORT void InitializeFromCurrentSelection(WebLocalFrame*,
                                                   size_t max_length);

  // Surrounding text content retrieved.
  BLINK_EXPORT WebString TextContent() const;

  // Offset in the text content of the initial hit position (or provided
  // offset in the node).
  // This should only be called when WebSurroundingText has been initialized
  // with a WebPoint.
  // DEPRECATED: use startOffsetInTextContent() or endOffsetInTextContent().
  BLINK_EXPORT size_t HitOffsetInTextContent() const;

  // Start offset of the initial text in the text content.
  BLINK_EXPORT size_t StartOffsetInTextContent() const;

  // End offset of the initial text in the text content.
  BLINK_EXPORT size_t EndOffsetInTextContent() const;

 protected:
  std::unique_ptr<SurroundingText> private_;
};

}  // namespace blink

#endif
