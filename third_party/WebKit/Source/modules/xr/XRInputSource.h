// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRInputSource_h
#define XRInputSource_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class XRSession;

class XRInputSource : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Handedness { kHandNone = 0, kHandLeft = 1, kHandRight = 2 };
  enum PointerOrigin { kOriginHead = 1, kOriginHand = 2, kOriginScreen = 3 };

  XRInputSource(XRSession*, uint32_t source_id);
  virtual ~XRInputSource() = default;

  XRSession* session() const { return session_; }

  const String& handedness() const { return handedness_string_; }
  const String& pointerOrigin() const { return pointer_origin_string_; }
  bool emulatedPosition() const { return emulated_position_; }

  uint32_t source_id() const { return source_id_; }

  void SetPointerOrigin(PointerOrigin);
  void SetHandedness(Handedness);
  void SetEmulatedPosition(bool emulated_position);
  void SetBasePoseMatrix(std::unique_ptr<TransformationMatrix>);
  void SetPointerTransformMatrix(std::unique_ptr<TransformationMatrix>);

  virtual void Trace(blink::Visitor*);

  int16_t active_frame_id = -1;
  bool primary_input_pressed = false;
  bool selection_cancelled = false;

 private:
  friend class XRPresentationFrame;

  const Member<XRSession> session_;
  const uint32_t source_id_;

  Handedness handedness_;
  String handedness_string_;

  PointerOrigin pointer_origin_;
  String pointer_origin_string_;

  bool emulated_position_ = false;

  std::unique_ptr<TransformationMatrix> base_pose_matrix_;

  // This is the transform to apply to the base_pose_matrix_ to get the pointer
  // matrix. In most cases it should be static.
  std::unique_ptr<TransformationMatrix> pointer_transform_matrix_;
};

}  // namespace blink

#endif  // XRInputSource_h
