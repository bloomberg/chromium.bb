// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InitModules_h
#define InitModules_h

#include "core/Init.h"

namespace blink {

class ModulesInitializer : public CoreInitializer {
public:
    virtual void init() override;
};

} // namespace blink

#endif // InitModules_h
