// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMenuSourceType_h
#define WebMenuSourceType_h

namespace blink {

enum WebMenuSourceType {
  kMenuSourceNone,
  kMenuSourceMouse,
  kMenuSourceKeyboard,
  kMenuSourceTouch,
  kMenuSourceTouchEditMenu,
  kMenuSourceLongPress,
  kMenuSourceLongTap,
  kMenuSourceTouchHandle,
  kMenuSourceStylus,
  kMenuSourceTypeLast = kMenuSourceStylus
};

}  // namespace blink

#endif
