// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_WORKAROUNDS_H_
#define UI_GL_GL_WORKAROUNDS_H_

namespace gl {

struct GLWorkarounds {
  // glClearColor does not always work on Intel 6xxx Mac drivers. See
  // crbug.com/710443.
  bool clear_to_zero_or_one_broken = false;
};

}  // namespace gl

#endif  // UI_GL_GL_WORKAROUNDS_H_
