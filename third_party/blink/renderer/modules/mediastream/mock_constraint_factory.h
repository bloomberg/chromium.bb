// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_CONSTRAINT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_CONSTRAINT_FACTORY_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// TODO(crbug.com/704136): Move this class out of the Blink exposed API
// when all users of it have been Onion souped.
class MockConstraintFactory {
 public:
  MockConstraintFactory();
  ~MockConstraintFactory();

  MediaConstraints CreateMediaConstraints() const;
  MediaTrackConstraintSetPlatform& basic() { return basic_; }
  MediaTrackConstraintSetPlatform& AddAdvanced();

  void DisableDefaultAudioConstraints();
  void DisableAecAudioConstraints();
  void Reset();

 private:
  MediaTrackConstraintSetPlatform basic_;
  Vector<MediaTrackConstraintSetPlatform> advanced_;

  DISALLOW_COPY_AND_ASSIGN(MockConstraintFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_CONSTRAINT_FACTORY_H_
