// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/activation_change_observer.h"

namespace aura {
namespace client {

// TODO(beng): For some reason, we need these in a separate .cc file for
//             dependent targets to link on windows!
ActivationChangeObserver::ActivationChangeObserver() {}
ActivationChangeObserver::~ActivationChangeObserver() {}

}  // namespace client
}  // namespace aura
