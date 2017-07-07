// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyboardEventManager_h
#define KeyboardEventManager_h

#include "build/build_config.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/Allocator.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class KeyboardEvent;
class LocalFrame;
class ScrollManager;
class WebKeyboardEvent;

enum class OverrideCapsLockState { kDefault, kOn, kOff };

class CORE_EXPORT KeyboardEventManager
    : public GarbageCollectedFinalized<KeyboardEventManager> {
  WTF_MAKE_NONCOPYABLE(KeyboardEventManager);

 public:
  static const int kAccessKeyModifiers =
// TODO(crbug.com/618397): Add a settings to control this behavior.
#if defined(OS_MACOSX)
      WebInputEvent::kControlKey | WebInputEvent::kAltKey;
#else
      WebInputEvent::kAltKey;
#endif

  KeyboardEventManager(LocalFrame&, ScrollManager&);
  DECLARE_TRACE();

  bool HandleAccessKey(const WebKeyboardEvent&);
  WebInputEventResult KeyEvent(const WebKeyboardEvent&);
  void DefaultKeyboardEventHandler(KeyboardEvent*, Node*);

  void CapsLockStateMayHaveChanged();
  static WebInputEvent::Modifiers GetCurrentModifierState();
  static bool CurrentCapsLockState();

 private:
  friend class Internals;
  // Allows overriding the current caps lock state for testing purposes.
  static void SetCurrentCapsLockState(OverrideCapsLockState);

  void DefaultSpaceEventHandler(KeyboardEvent*, Node*);
  void DefaultBackspaceEventHandler(KeyboardEvent*);
  void DefaultTabEventHandler(KeyboardEvent*);
  void DefaultEscapeEventHandler(KeyboardEvent*);
  void DefaultArrowEventHandler(KeyboardEvent*, Node*);

  const Member<LocalFrame> frame_;

  Member<ScrollManager> scroll_manager_;
};

}  // namespace blink

#endif  // KeyboardEventManager_h
