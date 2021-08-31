/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/display/mojom/display.mojom-blink.h"

namespace blink {

class LocalDOMWindow;

class CORE_EXPORT Screen : public EventTargetWithInlineData,
                           public ExecutionContextClient,
                           public Supplementable<Screen> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Screen(LocalDOMWindow*);

  virtual int height() const;
  virtual int width() const;
  virtual unsigned colorDepth() const;
  virtual unsigned pixelDepth() const;
  virtual int availLeft() const;
  virtual int availTop() const;
  virtual int availHeight() const;
  virtual int availWidth() const;

  void Trace(Visitor*) const override;

  // EventTargetWithInlineData:
  const WTF::AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Proposed: https://github.com/webscreens/window-placement
  // Whether this Screen is part of a multi-screen extended visual workspace.
  virtual bool isExtended() const;
  // An event fired when Screen attributes change.
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  // TODO(crbug.com/1116528): Move permission-gated attributes to an interface
  // that inherits from Screen: https://github.com/webscreens/window-placement
  Screen(display::mojom::blink::DisplayPtr display,
         bool internal,
         bool primary,
         const String& id);
  virtual int left() const;
  virtual int top() const;
  virtual bool internal() const;
  virtual bool primary() const;
  virtual float scaleFactor() const;
  virtual const String& id() const;
  virtual bool touchSupport() const;

  // Not web-exposed; for internal usage only.
  static constexpr int64_t kInvalidDisplayId = -1;
  virtual int64_t DisplayId() const;

 private:
  // A static snapshot of the display's information, provided upon construction.
  // This member is only non-null for Screen objects obtained via the
  // experimental Window Placement API.
  const display::mojom::blink::DisplayPtr display_;
  // True if this is an internal display of the device; it is a static value
  // provided upon construction. This member is only valid for Screen objects
  // obtained via the experimental Window Placement API.
  const absl::optional<bool> internal_;
  // True if this is the primary screen of the operating system; it is a static
  // value provided upon construction. This member is only valid for Screen
  // objects obtained via the experimental Window Placement API.
  const absl::optional<bool> primary_;
  // A web-exposed device id; it is a static value provided upon construction.
  // This member is only valid for Screen objects obtained via the experimental
  // Window Placement API.
  const String id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_H_
