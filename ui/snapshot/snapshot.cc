// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task_runner_util.h"
#include "ui/gfx/image/image.h"

namespace ui {

namespace {

scoped_refptr<base::RefCountedMemory> EncodeImage(const gfx::Image& image) {
  return image.As1xPNGBytes();
}

void EncodeImageAndSchedulePNGCallback(
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback,
    const gfx::Image& image) {
  base::PostTaskAndReplyWithResult(background_task_runner.get(), FROM_HERE,
                                   base::Bind(EncodeImage, image), callback);
}

}  // namespace

void GrabWindowSnapshotAsyncPNG(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    scoped_refptr<base::TaskRunner> background_task_runner,
    const GrabWindowSnapshotAsyncPNGCallback& callback) {
  GrabWindowSnapshotAsync(
      window, source_rect,
      base::Bind(EncodeImageAndSchedulePNGCallback,
                 std::move(background_task_runner), callback));
}

}  // namespace ui
