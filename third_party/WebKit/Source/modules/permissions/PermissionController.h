// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionController_h
#define PermissionController_h

#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"

namespace blink {

class MODULES_EXPORT PermissionController final
    : public GarbageCollectedFinalized<PermissionController>,
      public Supplement<LocalFrame>,
      public DOMWindowProperty {
  WTF_MAKE_NONCOPYABLE(PermissionController);
  USING_GARBAGE_COLLECTED_MIXIN(PermissionController);

 public:
  virtual ~PermissionController();

  static void provideTo(LocalFrame&);
  static PermissionController* from(LocalFrame&);
  static const char* supplementName();

  DECLARE_VIRTUAL_TRACE();

 private:
  PermissionController(LocalFrame&);

  // Inherited from DOMWindowProperty.
  void frameDestroyed() override;
};

}  // namespace blink

#endif  // PermissionController_h
