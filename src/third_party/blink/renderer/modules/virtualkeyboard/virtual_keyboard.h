// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VIRTUALKEYBOARD_VIRTUAL_KEYBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VIRTUALKEYBOARD_VIRTUAL_KEYBOARD_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace gfx {
class Rect;
}

namespace blink {

class ExecutionContext;

// The VirtualKeyboard API provides control of the on-screen keyboard
// to JS authors. The VirtualKeyboard object lives in the Navigator.
// It is exposed to JS via a new attribute virtualKeyboard in the Navigator.
class VirtualKeyboard final : public EventTargetWithInlineData,
                              public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VirtualKeyboard);

 public:
  explicit VirtualKeyboard(LocalFrame*);
  VirtualKeyboard(const VirtualKeyboard&) = delete;
  ~VirtualKeyboard() override;

  VirtualKeyboard& operator=(const VirtualKeyboard&) = delete;

  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(overlaygeometrychange, kOverlaygeometrychange)

  bool overlaysContent() const;
  void setOverlaysContent(bool overlays_content);

  void VirtualKeyboardOverlayChanged(const gfx::Rect&);

  // Public APIs for controlling the visibility of VirtualKeyboard.
  void show();
  void hide();

  void Trace(Visitor*) override;

 private:
  bool overlays_content_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VIRTUALKEYBOARD_VIRTUAL_KEYBOARD_H_
