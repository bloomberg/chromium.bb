// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/threaded_image_cursors.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/ui/common/image_cursors_set.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/platform_window/platform_window.h"

namespace ui {
namespace ws {
namespace {

void AddImageCursorsOnResourceThread(
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr,
    std::unique_ptr<ui::ImageCursors> image_cursors) {
  if (image_cursors_set_weak_ptr)
    image_cursors_set_weak_ptr->AddImageCursors(std::move(image_cursors));
}

void RemoveImageCursorsOnResourceThread(
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr,
    base::WeakPtr<ui::ImageCursors> image_cursors_weak_ptr) {
  if (image_cursors_set_weak_ptr && image_cursors_weak_ptr) {
    image_cursors_set_weak_ptr->DeleteImageCursors(
        image_cursors_weak_ptr.get());
  }
}

void SetDisplayOnResourceThread(
    base::WeakPtr<ui::ImageCursors> image_cursors_weak_ptr,
    const display::Display& display,
    float scale_factor) {
  if (image_cursors_weak_ptr)
    image_cursors_weak_ptr->SetDisplay(display, scale_factor);
}

void SetCursorSizeOnResourceThread(
    base::WeakPtr<ui::ImageCursors> image_cursors_weak_ptr,
    CursorSize cursor_size) {
  if (image_cursors_weak_ptr)
    image_cursors_weak_ptr->SetCursorSize(cursor_size);
}

// Executed on |resource_task_runner_|. Sets cursor type on
// |image_cursors_set_weak_ptr_|, and then schedules a task on
// |ui_service_task_runner_| to set the corresponding PlatformCursor on the
// provided |platform_window|.
// |platform_window| pointer needs to be valid while
// |threaded_image_cursors_weak_ptr| is not invalidated.
void SetCursorOnResourceThread(
    base::WeakPtr<ui::ImageCursors> image_cursors_weak_ptr,
    ui::CursorType cursor_type,
    ui::PlatformWindow* platform_window,
    scoped_refptr<base::SingleThreadTaskRunner> ui_service_task_runner_,
    base::WeakPtr<ThreadedImageCursors> threaded_image_cursors_weak_ptr) {
  if (image_cursors_weak_ptr) {
    ui::Cursor native_cursor(cursor_type);
    image_cursors_weak_ptr->SetPlatformCursor(&native_cursor);
    // |platform_window| is owned by the UI Service thread, so setting the
    // cursor on it also needs to happen on that thread.
    ui_service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ThreadedImageCursors::SetCursorOnPlatformWindow,
                              threaded_image_cursors_weak_ptr,
                              native_cursor.platform(), platform_window));
  }
}

}  // namespace

ThreadedImageCursors::ThreadedImageCursors(
    scoped_refptr<base::SingleThreadTaskRunner>& resource_task_runner,
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr)
    : resource_task_runner_(resource_task_runner),
      image_cursors_set_weak_ptr_(image_cursors_set_weak_ptr),
      weak_ptr_factory_(this) {
  DCHECK(resource_task_runner_);
  ui_service_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Create and initialize the ImageCursors object here and then set it on
  // |image_cursors_set_weak_ptr__|. Note that it is essential to initialize
  // the ImageCursors object on the UI Service's thread if we are using Ozone,
  // so that it uses the right (thread-local) CursorFactoryOzone instance.
  std::unique_ptr<ui::ImageCursors> image_cursors =
      base::MakeUnique<ui::ImageCursors>();
  image_cursors->Initialize();
  image_cursors_weak_ptr_ = image_cursors->GetWeakPtr();
  resource_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AddImageCursorsOnResourceThread, image_cursors_set_weak_ptr_,
                 base::Passed(std::move(image_cursors))));
}

ThreadedImageCursors::~ThreadedImageCursors() {
  resource_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RemoveImageCursorsOnResourceThread,
                 image_cursors_set_weak_ptr_, image_cursors_weak_ptr_));
}

void ThreadedImageCursors::SetDisplay(const display::Display& display,
                                      float scale_factor) {
  resource_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SetDisplayOnResourceThread,
                            image_cursors_weak_ptr_, display, scale_factor));
}

void ThreadedImageCursors::SetCursorSize(CursorSize cursor_size) {
  resource_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SetCursorSizeOnResourceThread,
                            image_cursors_weak_ptr_, cursor_size));
}

void ThreadedImageCursors::SetCursor(ui::CursorType cursor_type,
                                     ui::PlatformWindow* platform_window) {
  resource_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SetCursorOnResourceThread, image_cursors_weak_ptr_,
                 cursor_type, platform_window, ui_service_task_runner_,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ThreadedImageCursors::SetCursorOnPlatformWindow(
    ui::PlatformCursor platform_cursor,
    ui::PlatformWindow* platform_window) {
  platform_window->SetCursor(platform_cursor);
}

}  // namespace ws
}  // namespace ui
