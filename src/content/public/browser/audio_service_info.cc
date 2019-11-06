// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_service_info.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/audio_service_listener.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

base::ProcessId GetProcessIdForAudioService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // May be null in unittests.
  if (!BrowserMainLoop::GetInstance())
    return base::kNullProcessId;

  MediaStreamManager* manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  return manager->audio_service_listener()->GetProcessId();
}

}  // namespace content
