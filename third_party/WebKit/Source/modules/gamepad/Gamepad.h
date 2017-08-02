/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef Gamepad_h
#define Gamepad_h

#include "device/gamepad/public/cpp/gamepad.h"
#include "modules/gamepad/GamepadButton.h"
#include "modules/gamepad/GamepadPose.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Gamepad final : public GarbageCollectedFinalized<Gamepad>,
                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Gamepad* Create() { return new Gamepad; }
  ~Gamepad();

  typedef Vector<double> DoubleVector;

  const String& id() const { return id_; }
  void SetId(const String& id) { id_ = id; }

  unsigned index() const { return index_; }
  void SetIndex(unsigned val) { index_ = val; }

  bool connected() const { return connected_; }
  void SetConnected(bool val) { connected_ = val; }

  unsigned long long timestamp() const { return timestamp_; }
  void SetTimestamp(unsigned long long val) { timestamp_ = val; }

  const String& mapping() const { return mapping_; }
  void SetMapping(const String& val) { mapping_ = val; }

  const DoubleVector& axes();
  void SetAxes(unsigned count, const double* data);
  bool isAxisDataDirty() const { return is_axis_data_dirty_; }

  const GamepadButtonVector& buttons();
  void SetButtons(unsigned count, const device::GamepadButton* data);
  bool isButtonDataDirty() const { return is_button_data_dirty_; }

  GamepadPose* pose() const { return pose_; }
  void SetPose(const device::GamepadPose&);

  const String& hand() const { return hand_; }
  void SetHand(const device::GamepadHand&);

  unsigned displayId() const { return display_id_; }
  void SetDisplayId(unsigned val) { display_id_ = val; }

  DECLARE_TRACE();

 private:
  Gamepad();

  String id_;
  unsigned index_;
  bool connected_;
  unsigned long long timestamp_;
  String mapping_;
  DoubleVector axes_;
  GamepadButtonVector buttons_;
  Member<GamepadPose> pose_;
  String hand_;
  unsigned display_id_;
  bool is_axis_data_dirty_;
  bool is_button_data_dirty_;
};

}  // namespace blink

#endif  // Gamepad_h
