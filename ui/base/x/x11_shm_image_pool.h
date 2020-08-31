// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SHM_IMAGE_POOL_H_
#define UI_BASE_X_X11_SHM_IMAGE_POOL_H_

#include <cstring>
#include <list>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11.h"

namespace ui {

// Base class that creates XImages using shared memory that will be sent to
// XServer for processing. As Ozone and non-Ozone X11 have different
// PlatformEvent types, Ozone and non-Ozone provide own implementations that
// handles events. See AddEventDispatcher and RemoveEventDispatcher below.
class COMPONENT_EXPORT(UI_BASE_X) XShmImagePool
    : public base::RefCountedThreadSafe<XShmImagePool>,
      public XEventDispatcher {
 public:
  XShmImagePool(scoped_refptr<base::SequencedTaskRunner> host_task_runner,
                scoped_refptr<base::SequencedTaskRunner> event_task_runner,
                XDisplay* display,
                XID drawable,
                Visual* visual,
                int depth,
                std::size_t max_frames_pending);

  bool Resize(const gfx::Size& pixel_size);

  // Is XSHM supported by the server and are the shared buffers ready for use?
  bool Ready();

  // Obtain state for the current frame.
  SkBitmap& CurrentBitmap();
  SkCanvas* CurrentCanvas();
  XImage* CurrentImage();

  // Switch to the next cached frame.  CurrentBitmap() and CurrentImage() will
  // change to reflect the new frame.
  void SwapBuffers(base::OnceCallback<void(const gfx::Size&)> callback);

  // Part of setup and teardown must be done on the event task runner.  Posting
  // the tasks cannot be done in the constructor/destructor because because this
  // would cause subtle problems with the reference count for this object.  So
  // Initialize() must be called after constructing and Teardown() must be
  // called before destructing.
  void Initialize();
  void Teardown();

 protected:
  ~XShmImagePool() override;

  void DispatchShmCompletionEvent(XShmCompletionEvent event);

  bool CanDispatchXEvent(XEvent* xev);

  const scoped_refptr<base::SequencedTaskRunner> host_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> event_task_runner_;

#ifndef NDEBUG
  bool dispatcher_registered_ = false;
#endif

 private:
  friend class base::RefCountedThreadSafe<XShmImagePool>;

  struct FrameState {
    FrameState();
    ~FrameState();

    XShmSegmentInfo shminfo_{};
    bool shmem_attached_to_server_ = false;
    XScopedImage image;
    SkBitmap bitmap;
    std::unique_ptr<SkCanvas> canvas;
  };

  struct SwapClosure {
    SwapClosure();
    ~SwapClosure();

    base::OnceClosure closure;
    ShmSeg shmseg;
  };

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

  void InitializeOnGpu();
  void TeardownOnGpu();

  void Cleanup();

  XDisplay* const display_;
  const XID drawable_;
  Visual* const visual_;
  const int depth_;

  bool ready_ = false;
  gfx::Size pixel_size_;
  std::size_t frame_bytes_ = 0;
  std::vector<FrameState> frame_states_;
  std::size_t current_frame_index_ = 0;
  std::list<SwapClosure> swap_closures_;

  DISALLOW_COPY_AND_ASSIGN(XShmImagePool);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SHM_IMAGE_POOL_H_
