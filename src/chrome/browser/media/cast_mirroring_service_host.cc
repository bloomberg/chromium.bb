// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/cast_remoting_connector.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/mirroring/browser/single_client_video_capture_host.h"
#include "content/public/browser/audio_loopback_stream_creator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

using content::BrowserThread;

namespace mirroring {

namespace {

void CreateVideoCaptureHostOnIO(const std::string& device_id,
                                content::MediaStreamType type,
                                media::mojom::VideoCaptureHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  mojo::MakeStrongBinding(
      std::make_unique<SingleClientVideoCaptureHost>(
          device_id, type,
          base::BindRepeating(&content::VideoCaptureDeviceLauncher::
                                  CreateInProcessVideoCaptureDeviceLauncher,
                              std::move(device_task_runner))),
      std::move(request));
}

content::MediaStreamType ConvertVideoStreamType(
    content::DesktopMediaID::Type type) {
  switch (type) {
    case content::DesktopMediaID::TYPE_NONE:
      return content::MediaStreamType::MEDIA_NO_SERVICE;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      return content::MediaStreamType::MEDIA_GUM_TAB_VIDEO_CAPTURE;
    case content::DesktopMediaID::TYPE_SCREEN:
    case content::DesktopMediaID::TYPE_WINDOW:
      return content::MediaStreamType::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE;
  }

  // To suppress compiler warning on Windows.
  return content::MediaStreamType::MEDIA_NO_SERVICE;
}

// Get the content::WebContents associated with the given |id|.
content::WebContents* GetContents(
    const content::WebContentsMediaCaptureId& id) {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(id.render_process_id,
                                       id.main_render_frame_id));
}

content::DesktopMediaID BuildMediaIdForWebContents(
    content::WebContents* contents) {
  content::DesktopMediaID media_id;
  if (!contents)
    return media_id;
  media_id.type = content::DesktopMediaID::TYPE_WEB_CONTENTS;
  media_id.web_contents_id = content::WebContentsMediaCaptureId(
      contents->GetMainFrame()->GetProcess()->GetID(),
      contents->GetMainFrame()->GetRoutingID(),
      true /* enable_audio_throttling */, true /* disable_local_echo */);
  return media_id;
}

}  // namespace

// static
void CastMirroringServiceHost::GetForTab(
    content::WebContents* target_contents,
    mojom::MirroringServiceHostRequest request) {
  if (target_contents) {
    const content::DesktopMediaID media_id =
        BuildMediaIdForWebContents(target_contents);
    mojo::MakeStrongBinding(
        std::make_unique<mirroring::CastMirroringServiceHost>(media_id),
        std::move(request));
  }
}

// static
void CastMirroringServiceHost::GetForDesktop(
    content::WebContents* initiator_contents,
    const std::string& desktop_stream_id,
    mojom::MirroringServiceHostRequest request) {
  DCHECK(!desktop_stream_id.empty());
  if (initiator_contents) {
    std::string original_extension_name;
    const content::DesktopMediaID media_id =
        content::DesktopStreamsRegistry::GetInstance()->RequestMediaForStreamId(
            desktop_stream_id,
            initiator_contents->GetMainFrame()->GetProcess()->GetID(),
            initiator_contents->GetMainFrame()->GetRoutingID(),
            initiator_contents->GetVisibleURL().GetOrigin(),
            &original_extension_name, content::kRegistryStreamTypeDesktop);
    mojo::MakeStrongBinding(
        std::make_unique<mirroring::CastMirroringServiceHost>(media_id),
        std::move(request));
  }
}

CastMirroringServiceHost::CastMirroringServiceHost(
    content::DesktopMediaID source_media_id)
    : source_media_id_(source_media_id) {
  DCHECK(source_media_id_.type != content::DesktopMediaID::TYPE_NONE);
  // Observe the target WebContents for Tab/OffscreenTab mirroring.
  if (source_media_id_.type == content::DesktopMediaID::TYPE_WEB_CONTENTS)
    Observe(GetContents(source_media_id_.web_contents_id));
}

CastMirroringServiceHost::~CastMirroringServiceHost() {
  // TODO(xjz): Stop the mirroring if connected to the MirroringService.
  // Implementation will be added in a later CL.
}

void CastMirroringServiceHost::Start(
    mojom::SessionParametersPtr session_params,
    mojom::SessionObserverPtr observer,
    mojom::CastMessageChannelPtr outbound_channel,
    mojom::CastMessageChannelRequest inbound_channel) {
  // TODO(xjz): Connect to the Mirroring Service and start a mirroring session.
  // Implementation will be added in a later CL.
}

void CastMirroringServiceHost::GetVideoCaptureHost(
    media::mojom::VideoCaptureHostRequest request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CreateVideoCaptureHostOnIO, source_media_id_.ToString(),
                     ConvertVideoStreamType(source_media_id_.type),
                     std::move(request)));
}

void CastMirroringServiceHost::GetNetworkContext(
    network::mojom::NetworkContextRequest request) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  network_context_params->context_name = "mirroring";
  content::GetNetworkService()->CreateNetworkContext(
      std::move(request), std::move(network_context_params));
}

void CastMirroringServiceHost::CreateAudioStream(
    mojom::AudioStreamCreatorClientPtr client,
    const media::AudioParameters& params,
    uint32_t total_segments) {
  content::WebContents* source_web_contents = nullptr;
  if (source_media_id_.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    source_web_contents = web_contents();
    if (!source_web_contents) {
      VLOG(1) << "Failed to create audio stream: Invalid source.";
      return;
    }
  }

  if (!audio_stream_creator_) {
    audio_stream_creator_ = content::AudioLoopbackStreamCreator::
        CreateInProcessAudioLoopbackStreamCreator();
  }
  audio_stream_creator_->CreateLoopbackStream(
      source_web_contents, params, total_segments,
      base::BindRepeating(
          [](mojom::AudioStreamCreatorClientPtr client,
             media::mojom::AudioInputStreamPtr stream,
             media::mojom::AudioInputStreamClientRequest client_request,
             media::mojom::ReadOnlyAudioDataPipePtr data_pipe) {
            // TODO(xjz): Remove |initially_muted| argument from
            // mojom::AudioStreamCreatorClient::StreamCreated().
            client->StreamCreated(std::move(stream), std::move(client_request),
                                  std::move(data_pipe),
                                  false /* initially_muted */);
          },
          base::Passed(&client)));
}

void CastMirroringServiceHost::ConnectToRemotingSource(
    media::mojom::RemoterPtr remoter,
    media::mojom::RemotingSourceRequest request) {
  if (source_media_id_.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    content::WebContents* const source_contents = web_contents();
    if (source_contents) {
      CastRemotingConnector::Get(source_contents)
          ->ConnectWithMediaRemoter(std::move(remoter), std::move(request));
    }
  }
}

void CastMirroringServiceHost::WebContentsDestroyed() {
  // TODO(xjz): Stop the mirroring if connected to the MirroringService.
  // Implementation will be added in a later CL.
}

}  // namespace mirroring
