// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/pepper_test_plugin/demo_3d.h"

#if !defined(INDEPENDENT_PLUGIN)
namespace pepper {
Demo3D::Demo3D() {
  esInitContext(&context_);
  memset(&user_data_, 0, sizeof(HTUserData));
  context_.userData = &user_data_;
}

Demo3D::~Demo3D() {
}

void Demo3D::SetWindowSize(int width, int height) {
  context_.width = width;
  context_.height = height;
}

bool Demo3D::InitGL() {
  return htInit(&context_);
}

void Demo3D::ReleaseGL() {
  htShutDown(&context_);
}

void Demo3D::Draw() {
  htDraw(&context_);
}
}  // namespace pepper
#endif  // INDEPENDENT_PLUGIN
