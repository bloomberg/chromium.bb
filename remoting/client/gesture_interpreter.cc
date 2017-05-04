// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gesture_interpreter.h"

#include "base/logging.h"

namespace remoting {

GestureInterpreter::GestureInterpreter(
    const DesktopViewport::TransformationCallback& on_transformation_changed) {
  viewport_.RegisterOnTransformationChangedCallback(on_transformation_changed,
                                                    true);
}

GestureInterpreter::~GestureInterpreter() {}

void GestureInterpreter::Pinch(float pivot_x, float pivot_y, float scale) {
  viewport_.ScaleDesktop(pivot_x, pivot_y, scale);
}

void GestureInterpreter::Pan(float translation_x, float translation_y) {
  // TODO(yuweih): Handle trackpad mode where the viewport always focus on the
  //     cursor.
  viewport_.MoveDesktop(translation_x, translation_y);
}

void GestureInterpreter::Tap(float x, float y) {
  NOTIMPLEMENTED();
}

void GestureInterpreter::LongPress(float x, float y) {
  NOTIMPLEMENTED();
}

void GestureInterpreter::OnSurfaceSizeChanged(int width, int height) {
  viewport_.SetSurfaceSize(width, height);
}

void GestureInterpreter::OnDesktopSizeChanged(int width, int height) {
  viewport_.SetDesktopSize(width, height);
}

}  // namespace remoting
