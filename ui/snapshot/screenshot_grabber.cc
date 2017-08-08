// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/screenshot_grabber.h"

#include <stddef.h>

#include <climits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

#if defined(USE_AURA)
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#endif

namespace ui {

namespace {
// The minimum interval between two screenshot commands.  It has to be
// more than 1000 to prevent the conflict of filenames.
const int kScreenshotMinimumIntervalInMS = 1000;

using ShowNotificationCallback =
    base::Callback<void(ScreenshotGrabberObserver::Result screenshot_result,
                        const base::FilePath& screenshot_path)>;

void SaveScreenshot(scoped_refptr<base::TaskRunner> ui_task_runner,
                    const ShowNotificationCallback& callback,
                    const base::FilePath& screenshot_path,
                    scoped_refptr<base::RefCountedMemory> png_data,
                    ScreenshotGrabberDelegate::FileResult result,
                    const base::FilePath& local_path) {
  DCHECK(!base::MessageLoopForUI::IsCurrent());
  DCHECK(!screenshot_path.empty());

  // Convert FileResult into ScreenshotGrabberObserver::Result.
  ScreenshotGrabberObserver::Result screenshot_result =
      ScreenshotGrabberObserver::SCREENSHOT_SUCCESS;
  switch (result) {
    case ScreenshotGrabberDelegate::FILE_SUCCESS:
      // Successfully got a local file to write to, write png data.
      DCHECK_GT(static_cast<int>(png_data->size()), 0);
      if (static_cast<size_t>(base::WriteFile(
              local_path, reinterpret_cast<const char*>(png_data->front()),
              static_cast<int>(png_data->size()))) != png_data->size()) {
        LOG(ERROR) << "Failed to save to " << local_path.value();
        screenshot_result =
            ScreenshotGrabberObserver::SCREENSHOT_WRITE_FILE_FAILED;
      }
      break;
    case ScreenshotGrabberDelegate::FILE_CHECK_DIR_FAILED:
      screenshot_result =
          ScreenshotGrabberObserver::SCREENSHOT_CHECK_DIR_FAILED;
      break;
    case ScreenshotGrabberDelegate::FILE_CREATE_DIR_FAILED:
      screenshot_result =
          ScreenshotGrabberObserver::SCREENSHOT_CREATE_DIR_FAILED;
      break;
    case ScreenshotGrabberDelegate::FILE_CREATE_FAILED:
      screenshot_result =
          ScreenshotGrabberObserver::SCREENSHOT_CREATE_FILE_FAILED;
      break;
  }

  // Report the result on the UI thread.
  ui_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, screenshot_result, screenshot_path));
}

void EnsureLocalDirectoryExists(
    const base::FilePath& path,
    ScreenshotGrabberDelegate::FileCallback callback) {
  DCHECK(!base::MessageLoopForUI::IsCurrent());
  DCHECK(!path.empty());

  if (!base::CreateDirectory(path.DirName())) {
    LOG(ERROR) << "Failed to ensure the existence of "
               << path.DirName().value();
    callback.Run(ScreenshotGrabberDelegate::FILE_CREATE_DIR_FAILED, path);
    return;
  }

  callback.Run(ScreenshotGrabberDelegate::FILE_SUCCESS, path);
}

}  // namespace

void ScreenshotGrabberDelegate::PrepareFileAndRunOnBlockingPool(
    const base::FilePath& path,
    const FileCallback& callback_on_blocking_pool) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::Bind(EnsureLocalDirectoryExists, path, callback_on_blocking_pool));
}

#if defined(USE_AURA)
class ScreenshotGrabber::ScopedCursorHider {
 public:
  // The nullptr might be returned when GetCursorClient is nullptr.
  static std::unique_ptr<ScopedCursorHider> Create(aura::Window* window) {
    DCHECK(window->IsRootWindow());
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window);
    if (!cursor_client)
      return nullptr;
    cursor_client->HideCursor();
    return std::unique_ptr<ScopedCursorHider>(
        base::WrapUnique(new ScopedCursorHider(window)));
  }

  ~ScopedCursorHider() {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window_);
    cursor_client->ShowCursor();
  }

 private:
  explicit ScopedCursorHider(aura::Window* window) : window_(window) {}
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorHider);
};
#endif

ScreenshotGrabber::ScreenshotGrabber(ScreenshotGrabberDelegate* client)
    : client_(client), factory_(this) {}

ScreenshotGrabber::~ScreenshotGrabber() {
}

void ScreenshotGrabber::TakeScreenshot(gfx::NativeWindow window,
                                       const gfx::Rect& rect,
                                       const base::FilePath& screenshot_path) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  last_screenshot_timestamp_ = base::TimeTicks::Now();

  bool is_partial = true;
  // Window identifier is used to log a message on failure to capture a full
  // screen (i.e. non partial) screenshot. The only time is_partial can be
  // false, we will also have an identification string for the window.
  std::string window_identifier;
#if defined(USE_AURA)
  aura::Window* aura_window = static_cast<aura::Window*>(window);
  is_partial = rect.size() != aura_window->bounds().size();
  window_identifier = aura_window->GetBoundsInScreen().ToString();

  cursor_hider_ = ScopedCursorHider::Create(aura_window->GetRootWindow());
#endif
  ui::GrabWindowSnapshotAsyncPNG(
      window, rect,
      base::Bind(&ScreenshotGrabber::GrabWindowSnapshotAsyncCallback,
                 factory_.GetWeakPtr(), window_identifier, screenshot_path,
                 is_partial));
}

bool ScreenshotGrabber::CanTakeScreenshot() {
  return last_screenshot_timestamp_.is_null() ||
         base::TimeTicks::Now() - last_screenshot_timestamp_ >
             base::TimeDelta::FromMilliseconds(kScreenshotMinimumIntervalInMS);
}

void ScreenshotGrabber::NotifyScreenshotCompleted(
    ScreenshotGrabberObserver::Result screenshot_result,
    const base::FilePath& screenshot_path) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
#if defined(USE_AURA)
  cursor_hider_.reset();
#endif
  for (ScreenshotGrabberObserver& observer : observers_)
    observer.OnScreenshotCompleted(screenshot_result, screenshot_path);
}

void ScreenshotGrabber::AddObserver(ScreenshotGrabberObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenshotGrabber::RemoveObserver(ScreenshotGrabberObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ScreenshotGrabber::HasObserver(
    const ScreenshotGrabberObserver* observer) const {
  return observers_.HasObserver(observer);
}

void ScreenshotGrabber::GrabWindowSnapshotAsyncCallback(
    const std::string& window_identifier,
    base::FilePath screenshot_path,
    bool is_partial,
    scoped_refptr<base::RefCountedMemory> png_data) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!png_data.get()) {
    if (is_partial) {
      LOG(ERROR) << "Failed to grab the window screenshot";
      NotifyScreenshotCompleted(
          ScreenshotGrabberObserver::SCREENSHOT_GRABWINDOW_PARTIAL_FAILED,
          screenshot_path);
    } else {
      LOG(ERROR) << "Failed to grab the window screenshot for "
                 << window_identifier;
      NotifyScreenshotCompleted(
          ScreenshotGrabberObserver::SCREENSHOT_GRABWINDOW_FULL_FAILED,
          screenshot_path);
    }
    return;
  }

  ShowNotificationCallback notification_callback(base::Bind(
      &ScreenshotGrabber::NotifyScreenshotCompleted, factory_.GetWeakPtr()));
  client_->PrepareFileAndRunOnBlockingPool(
      screenshot_path,
      base::Bind(&SaveScreenshot, base::ThreadTaskRunnerHandle::Get(),
                 notification_callback, screenshot_path, png_data));
}

}  // namespace ui
