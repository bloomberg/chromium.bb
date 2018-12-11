// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_listener.h"

#include "ui/base/accelerators/global_media_keys_listener_win.h"

namespace ui {

std::unique_ptr<MediaKeysListener> MediaKeysListener::Create(
    MediaKeysListener::Delegate* delegate,
    MediaKeysListener::Scope scope) {
  DCHECK(delegate);

  if (scope == Scope::kGlobal) {
    if (!GlobalMediaKeysListenerWin::has_instance())
      return std::make_unique<GlobalMediaKeysListenerWin>(delegate);
    // We shouldn't try to create more than one GlobalMediaKeysListenerWin
    // instance.
    NOTREACHED();
  }
  return nullptr;
}

}  // namespace ui