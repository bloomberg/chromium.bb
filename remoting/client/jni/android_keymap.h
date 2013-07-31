// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_ANDROID_KEYMAP_H_
#define REMOTING_CLIENT_JNI_ANDROID_KEYMAP_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

// This is a temporary hack to convert Android's virtual keycodes into USB
// scancodes. Unfortunately, the current solution uses key mappings whose
// layout is determined according to the client device's input locale instead
// of the host's internationalization preferences. The whole process needs to
// be rethought to accomplish this in an localizable and future-proof way.
// TODO(solb): crbug.com/265945
uint32 AndroidKeycodeToUsbKeycode(int android);

}  // namespace remoting

#endif
