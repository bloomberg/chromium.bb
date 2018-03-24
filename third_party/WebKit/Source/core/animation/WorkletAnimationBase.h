// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletAnimationBase_h
#define WorkletAnimationBase_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Document;
class KeyframeEffect;

class CORE_EXPORT WorkletAnimationBase : public ScriptWrappable {
 public:
  virtual ~WorkletAnimationBase() = default;

  // Attempts to start the animation on the compositor side, returning true if
  // it succeeds or false otherwise. If false is returned and failure_message
  // was non-null, failure_message may be filled with an error description.
  //
  // On a false return it may still be possible to start the animation on the
  // compositor later (e.g. if an incompatible property is removed from the
  // element), so the caller should try again next main frame.
  virtual bool StartOnCompositor(String* failure_message) = 0;

  virtual Document* GetDocument() const = 0;
  virtual KeyframeEffect* GetEffect() const = 0;
};

}  // namespace blink

#endif  // WorkletAnimationBase_h
