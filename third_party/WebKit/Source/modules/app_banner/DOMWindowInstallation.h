// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowInstallation_h
#define DOMWindowInstallation_h

#include "core/dom/events/EventTarget.h"
#include "core/event_type_names.h"

namespace blink {

class DOMWindowInstallation {
 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(appinstalled);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(beforeinstallprompt);
};

}  // namespace blink

#endif  // DOMWindowInstallation_h
