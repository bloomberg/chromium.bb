// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/in_process_audio_loopback_stream_creator.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

InProcessAudioLoopbackStreamCreator::InProcessAudioLoopbackStreamCreator()
    : factory_(nullptr,
               content::ServiceManagerConnection::GetForProcess()
                   ->GetConnector()
                   ->Clone(),
               AudioStreamBrokerFactory::CreateImpl()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

InProcessAudioLoopbackStreamCreator::~InProcessAudioLoopbackStreamCreator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  factory_.FrameDeleted(nullptr);
}

void InProcessAudioLoopbackStreamCreator::CreateLoopbackStream(
    WebContents* loopback_source,
    const media::AudioParameters& params,
    uint32_t total_segments,
    const StreamCreatedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* loopback_source_frame = nullptr;
  if (loopback_source) {
    loopback_source_frame = loopback_source->GetMainFrame();
    DCHECK(loopback_source_frame);
  }
  factory_.CreateInProcessLoopbackStream(loopback_source_frame, params,
                                         total_segments, callback);
}

}  // namespace content
