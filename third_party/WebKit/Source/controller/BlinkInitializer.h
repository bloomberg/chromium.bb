// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkInitializer_h
#define BlinkInitializer_h

#include "modules/ModulesInitializer.h"

namespace blink {

class BlinkInitializer : public ModulesInitializer {
 public:
  void RegisterInterfaces(service_manager::BinderRegistry&) override;
  void OnClearWindowObjectInMainWorld(Document&,
                                      const Settings&) const override;
  void InitLocalFrame(LocalFrame&) const override;
};

}  // namespace blink

#endif  // BlinkInitializer_h
