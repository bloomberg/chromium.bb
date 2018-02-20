/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ColorChooserClient_h
#define ColorChooserClient_h

#include "core/CoreExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "third_party/WebKit/public/mojom/color_chooser/color_chooser.mojom-blink.h"

namespace blink {

class Element;

class CORE_EXPORT ColorChooserClient : public GarbageCollectedMixin {
 public:
  virtual ~ColorChooserClient();
  void Trace(blink::Visitor* visitor) override {}

  virtual void DidChooseColor(const Color&) = 0;
  virtual void DidEndChooser() = 0;
  virtual Element& OwnerElement() const = 0;
  virtual IntRect ElementRectRelativeToViewport() const = 0;
  virtual Color CurrentColor() = 0;
  virtual bool ShouldShowSuggestions() const = 0;
  virtual Vector<mojom::blink::ColorSuggestionPtr> Suggestions() const = 0;
};

}  // namespace blink

#endif  // ColorChooserClient_h
