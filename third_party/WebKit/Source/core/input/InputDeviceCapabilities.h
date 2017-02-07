// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InputDeviceCapabilities_h
#define InputDeviceCapabilities_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/input/InputDeviceCapabilitiesInit.h"

namespace blink {

class CORE_EXPORT InputDeviceCapabilities final
    : public GarbageCollected<InputDeviceCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static InputDeviceCapabilities* create(bool firesTouchEvents) {
    return new InputDeviceCapabilities(firesTouchEvents);
  }

  static InputDeviceCapabilities* create(
      const InputDeviceCapabilitiesInit& initializer) {
    return new InputDeviceCapabilities(initializer);
  }

  bool firesTouchEvents() const { return m_firesTouchEvents; }

  DEFINE_INLINE_TRACE() {}

 private:
  InputDeviceCapabilities(bool firesTouchEvents);
  InputDeviceCapabilities(const InputDeviceCapabilitiesInit&);

  // Whether this device dispatches touch events. This mainly lets developers
  // avoid handling both touch and mouse events dispatched for a single user
  // action.
  bool m_firesTouchEvents;
};

// Grouping constant-valued InputDeviceCapabilities objects together,
// which is kept and used by each 'view' (DOMWindow) that dispatches
// events parameterized over InputDeviceCapabilities.
//
// TODO(sof): lazily instantiate InputDeviceCapabilities instances upon
// UIEvent access instead. This would allow internal tracking of such
// capabilities by value.
class InputDeviceCapabilitiesConstants final
    : public GarbageCollected<InputDeviceCapabilitiesConstants> {
 public:
  // Returns an InputDeviceCapabilities which has
  // |firesTouchEvents| set to value of |firesTouch|.
  InputDeviceCapabilities* firesTouchEvents(bool firesTouch);

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_firesTouchEvents);
    visitor->trace(m_doesntFireTouchEvents);
  }

 private:
  Member<InputDeviceCapabilities> m_firesTouchEvents;
  Member<InputDeviceCapabilities> m_doesntFireTouchEvents;
};

}  // namespace blink

#endif  // InputDeviceCapabilities_h
