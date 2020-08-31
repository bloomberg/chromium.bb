// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/mirroring/browser/cast_remoting_sender.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/media_router/media_remoting_dialog_view.h"
#endif

using content::BrowserThread;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;
using media::mojom::RemotingSinkMetadata;

class CastRemotingConnector::RemotingBridge : public media::mojom::Remoter {
 public:
  // Constructs a "bridge" to delegate calls between the given |source| and
  // |connector|. |connector| must be valid at the time of construction, but is
  // otherwise a weak pointer that can become invalid during the lifetime of a
  // RemotingBridge.
  RemotingBridge(mojo::PendingRemote<media::mojom::RemotingSource> source,
                 CastRemotingConnector* connector)
      : source_(std::move(source)), connector_(connector) {
    DCHECK(connector_);
    source_.set_disconnect_handler(
        base::BindOnce(&RemotingBridge::Stop, base::Unretained(this),
                       RemotingStopReason::SOURCE_GONE));
    connector_->RegisterBridge(this);
  }

  ~RemotingBridge() final {
    if (connector_)
      connector_->DeregisterBridge(this, RemotingStopReason::SOURCE_GONE);
  }

  // The CastRemotingConnector calls these to call back to the RemotingSource.
  void OnSinkAvailable(const RemotingSinkMetadata& metadata) {
    source_->OnSinkAvailable(metadata.Clone());
  }
  void OnSinkGone() { source_->OnSinkGone(); }
  void OnStarted() { source_->OnStarted(); }
  void OnStartFailed(RemotingStartFailReason reason) {
    source_->OnStartFailed(reason);
  }
  void OnMessageFromSink(const std::vector<uint8_t>& message) {
    source_->OnMessageFromSink(message);
  }
  void OnStopped(RemotingStopReason reason) { source_->OnStopped(reason); }

  // The CastRemotingConnector calls this when it is no longer valid.
  void OnCastRemotingConnectorDestroyed() {
    connector_ = nullptr;
  }

  // media::mojom::Remoter implementation. The source calls these to start/stop
  // media remoting and send messages to the sink. These simply delegate to the
  // CastRemotingConnector, which mediates to establish only one remoting
  // session among possibly multiple requests. The connector will respond to
  // this request by calling one of: OnStarted() or OnStartFailed().
  void Start() final {
    if (connector_)
      connector_->StartRemoting(this);
  }
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender) final {
    if (connector_) {
      connector_->StartRemotingDataStreams(
          this, std::move(audio_pipe), std::move(video_pipe),
          std::move(audio_sender), std::move(video_sender));
    }
  }
  void Stop(RemotingStopReason reason) final {
    if (connector_)
      connector_->StopRemoting(this, reason, true);
  }
  void SendMessageToSink(const std::vector<uint8_t>& message) final {
    if (connector_)
      connector_->SendMessageToSink(this, message);
  }
  void EstimateTransmissionCapacity(
      media::mojom::Remoter::EstimateTransmissionCapacityCallback callback)
      final {
    if (connector_)
      connector_->EstimateTransmissionCapacity(std::move(callback));
    else
      std::move(callback).Run(0);
  }

 private:
  mojo::Remote<media::mojom::RemotingSource> source_;

  // Weak pointer. Will be set to nullptr if the CastRemotingConnector is
  // destroyed before this RemotingBridge.
  CastRemotingConnector* connector_;

  DISALLOW_COPY_AND_ASSIGN(RemotingBridge);
};

// static
const void* const CastRemotingConnector::kUserDataKey = &kUserDataKey;

// static
CastRemotingConnector* CastRemotingConnector::Get(
    content::WebContents* contents) {
  DCHECK(contents);
  CastRemotingConnector* connector =
      static_cast<CastRemotingConnector*>(contents->GetUserData(kUserDataKey));
  if (!connector) {
    if (!media_router::MediaRouterEnabled(contents->GetBrowserContext()))
      return nullptr;
    connector = new CastRemotingConnector(
        media_router::MediaRouterFactory::GetApiForBrowserContext(
            contents->GetBrowserContext()),
        Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs(),
        sessions::SessionTabHelper::IdForTab(contents),
#if defined(TOOLKIT_VIEWS)
        base::BindRepeating(
            [](content::WebContents* contents,
               PermissionResultCallback result_callback) {
              media_router::MediaRemotingDialogView::GetPermission(
                  contents, std::move(result_callback));
              return media_router::MediaRemotingDialogView::IsShowing()
                         ? base::BindOnce(
                               &media_router::MediaRemotingDialogView::
                                   HideDialog)
                         : CancelPermissionRequestCallback();
            },
            contents)
#else
        base::BindRepeating([](PermissionResultCallback result_callback) {
          std::move(result_callback).Run(true);
          return CancelPermissionRequestCallback();
        })
#endif
    );
    contents->SetUserData(kUserDataKey, base::WrapUnique(connector));
  }
  return connector;
}

// static
void CastRemotingConnector::CreateMediaRemoter(
    content::RenderFrameHost* host,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  DCHECK(host);
  auto* const contents = content::WebContents::FromRenderFrameHost(host);
  if (!contents)
    return;
  CastRemotingConnector* const connector = CastRemotingConnector::Get(contents);
  if (!connector)
    return;
  connector->CreateBridge(std::move(source), std::move(receiver));
}

CastRemotingConnector::CastRemotingConnector(
    media_router::MediaRouter* router,
    PrefService* pref_service,
    SessionID tab_id,
    PermissionRequestCallback permission_request_callback)
    : media_router_(router),
      tab_id_(tab_id),
      permission_request_callback_(std::move(permission_request_callback)),
      active_bridge_(nullptr),
      pref_service_(pref_service) {
  DCHECK(permission_request_callback_);
  StartObservingPref();
}

CastRemotingConnector::~CastRemotingConnector() {
  // Assume nothing about destruction/shutdown sequence of a tab. For example,
  // it's possible the owning WebContents will be destroyed before the Mojo
  // message pipes to the RemotingBridges have been closed.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::ROUTE_TERMINATED, false);
  for (RemotingBridge* notifyee : bridges_) {
    notifyee->OnSinkGone();
    notifyee->OnCastRemotingConnectorDestroyed();
  }
}

void CastRemotingConnector::ConnectToService(
    mojo::PendingReceiver<media::mojom::MirrorServiceRemotingSource>
        source_receiver,
    mojo::PendingRemote<media::mojom::MirrorServiceRemoter> remoter) {
  DCHECK(!deprecated_remoter_);
  DCHECK(remoter);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  deprecated_receiver_.Bind(std::move(source_receiver));
  deprecated_receiver_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
  deprecated_remoter_.Bind(std::move(remoter));
  deprecated_remoter_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
}

void CastRemotingConnector::ResetRemotingPermission() {
  remoting_allowed_.reset();
}

void CastRemotingConnector::ConnectWithMediaRemoter(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
  DCHECK(!remoter_);
  DCHECK(remoter);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
  remoter_.Bind(std::move(remoter));
  remoter_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
}

void CastRemotingConnector::OnMirrorServiceStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;

  deprecated_receiver_.reset();
  deprecated_remoter_.reset();
  receiver_.reset();
  remoter_.reset();

  sink_metadata_ = RemotingSinkMetadata();
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::SERVICE_GONE, false);
  for (RemotingBridge* notifyee : bridges_)
    notifyee->OnSinkGone();
}

void CastRemotingConnector::CreateBridge(
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemotingBridge>(std::move(source), this),
      std::move(receiver));
}

void CastRemotingConnector::RegisterBridge(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) == bridges_.end());

  bridges_.insert(bridge);
  if ((deprecated_remoter_ || remoter_) && !active_bridge_ &&
      remoting_allowed_.value_or(true))
    bridge->OnSinkAvailable(sink_metadata_);
}

void CastRemotingConnector::DeregisterBridge(RemotingBridge* bridge,
                                             RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) != bridges_.end());

  bridges_.erase(bridge);
  if (bridge == active_bridge_)
    StopRemoting(bridge, reason, true);
}

void CastRemotingConnector::StartRemoting(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) != bridges_.end());

  // Refuse to start if there is no remoting route available, or if remoting is
  // already active.
  if (!deprecated_remoter_ && !remoter_) {
    DVLOG(2) << "Remoting start failed: Invalid ANSWER message.";
    bridge->OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
    return;
  }
  if (active_bridge_) {
    DVLOG(2) << "Remoting start failed: Cannot start multiple.";
    bridge->OnStartFailed(RemotingStartFailReason::CANNOT_START_MULTIPLE);
    return;
  }

  // Notify all other sources that the sink is no longer available for remoting.
  // A race condition is possible, where one of the other sources will try to
  // start remoting before receiving this notification; but that attempt will
  // just fail later on.
  for (RemotingBridge* notifyee : bridges_) {
    if (notifyee == bridge)
      continue;
    notifyee->OnSinkGone();
  }

  active_bridge_ = bridge;

  if (remoting_allowed_.has_value()) {
    StartRemotingIfPermitted();
  } else {
    PermissionResultCallback dialog_result_callback(base::BindOnce(
        [](base::WeakPtr<CastRemotingConnector> connector, bool is_allowed) {
          DCHECK_CURRENTLY_ON(BrowserThread::UI);
          if (!connector)
            return;
          connector->permission_request_cancel_callback_.Reset();
          connector->remoting_allowed_ = is_allowed;
          connector->StartRemotingIfPermitted();
        },
        weak_factory_.GetWeakPtr()));

    DCHECK(!permission_request_cancel_callback_);
    permission_request_cancel_callback_ =
        permission_request_callback_.Run(std::move(dialog_result_callback));
  }
}

void CastRemotingConnector::StartRemotingIfPermitted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!active_bridge_)
    return;

  if (remoting_allowed_.value()) {
    if (deprecated_remoter_) {
      DCHECK(!remoter_);
      deprecated_remoter_->Start();

      // Assume the remoting session is always started successfully. If any
      // failure occurs, OnError() will be called.
      active_bridge_->OnStarted();
    } else {
      DCHECK(remoter_);
      remoter_->Start();
    }
  } else {
    // TODO(crbug.com/1015486): Add an extra reason for this failure.
    active_bridge_->OnStartFailed(RemotingStartFailReason::ROUTE_TERMINATED);
    active_bridge_->OnSinkGone();
    active_bridge_ = nullptr;
  }
}

void CastRemotingConnector::StartRemotingDataStreams(
    RemotingBridge* bridge,
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Refuse to start if there is no remoting route available, or if remoting is
  // not active for this |bridge|.
  if ((!deprecated_remoter_ && !remoter_) || active_bridge_ != bridge)
    return;
  // Also, if neither audio nor video pipe was provided, or if a receiver for a
  // RemotingDataStreamSender was not provided for a data pipe, error-out early.
  if ((!audio_pipe.is_valid() && !video_pipe.is_valid()) ||
      (audio_pipe.is_valid() && !audio_sender.is_valid()) ||
      (video_pipe.is_valid() && !video_sender.is_valid())) {
    StopRemoting(active_bridge_, RemotingStopReason::DATA_SEND_FAILED, false);
    return;
  }

  if (deprecated_remoter_) {
    DCHECK(!remoter_);
    const bool want_audio = audio_sender.is_valid();
    const bool want_video = video_sender.is_valid();
    deprecated_remoter_->StartDataStreams(
        want_audio, want_video,
        base::BindOnce(&CastRemotingConnector::OnDataStreamsStarted,
                       weak_factory_.GetWeakPtr(), std::move(audio_pipe),
                       std::move(video_pipe), std::move(audio_sender),
                       std::move(video_sender)));
  } else {
    DCHECK(remoter_);
    remoter_->StartDataStreams(std::move(audio_pipe), std::move(video_pipe),
                               std::move(audio_sender),
                               std::move(video_sender));
  }
}

void CastRemotingConnector::OnDataStreamsStarted(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> video_sender,
    int32_t audio_stream_id,
    int32_t video_stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(deprecated_remoter_);
  DCHECK(!remoter_);

  if (!active_bridge_) {
    deprecated_remoter_->Stop(media::mojom::RemotingStopReason::SOURCE_GONE);
    return;
  }

  if (audio_sender && audio_stream_id > -1) {
    mirroring::CastRemotingSender::FindAndBind(
        audio_stream_id, std::move(audio_pipe), std::move(audio_sender),
        base::BindOnce(&CastRemotingConnector::OnDataSendFailed,
                       weak_factory_.GetWeakPtr()));
  }
  if (video_sender && video_stream_id > -1) {
    mirroring::CastRemotingSender::FindAndBind(
        video_stream_id, std::move(video_pipe), std::move(video_sender),
        base::BindOnce(&CastRemotingConnector::OnDataSendFailed,
                       weak_factory_.GetWeakPtr()));
  }
}

void CastRemotingConnector::StopRemoting(RemotingBridge* bridge,
                                         RemotingStopReason reason,
                                         bool is_initiated_by_source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (active_bridge_ != bridge)
    return;

  active_bridge_ = nullptr;

  // Cancel all outstanding callbacks related to the remoting session.
  weak_factory_.InvalidateWeakPtrs();

  if (permission_request_cancel_callback_) {
    std::move(permission_request_cancel_callback_).Run();
    if (is_initiated_by_source && remoter_) {
      // The source requested remoting be stopped before the permission request
      // was resolved. This means the |remoter_| was never started, and remains
      // in the available state, still all ready to go. Thus, notify the sources
      // that the sink is available once again.
      for (RemotingBridge* notifyee : bridges_)
        notifyee->OnSinkAvailable(sink_metadata_);
    }
    return;  // Early returns since the |remoter_| was never started.
  }

  // Reset |sink_metadata_|. Remoting can only be started after
  // OnSinkAvailable() is called again.
  sink_metadata_ = RemotingSinkMetadata();

  // Prevent the source from trying to start again until the Cast Provider has
  // indicated the stop operation has completed.
  bridge->OnSinkGone();
  // Note: At this point, all sources should think the sink is gone.

  if (deprecated_remoter_) {
    DCHECK(!remoter_);
    deprecated_remoter_->Stop(reason);
  }
  if (remoter_) {
    DCHECK(!deprecated_remoter_);
    remoter_->Stop(reason);
  }

  bridge->OnStopped(reason);
}

void CastRemotingConnector::OnStopped(RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (active_bridge_) {
    // This call will reset |sink_metadata_| and notify the source that sink is
    // gone.
    StopRemoting(active_bridge_, reason, false);
  } else if (reason == RemotingStopReason::USER_DISABLED) {
    // Notify all the sources that the sink is gone. Remoting can only be
    // started after OnSinkAvailable() is called again.
    sink_metadata_ = RemotingSinkMetadata();
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkGone();
  }
}

void CastRemotingConnector::SendMessageToSink(
    RemotingBridge* bridge, const std::vector<uint8_t>& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // During an active remoting session, simply pass all binary messages through
  // to the sink.
  if ((!deprecated_remoter_ && !remoter_) || active_bridge_ != bridge)
    return;
  if (deprecated_remoter_) {
    DCHECK(!remoter_);
    deprecated_remoter_->SendMessageToSink(message);
  } else {
    DCHECK(!deprecated_remoter_);
    remoter_->SendMessageToSink(message);
  }
}

void CastRemotingConnector::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // During an active remoting session, simply pass all binary messages through
  // to the source.
  if (!active_bridge_)
    return;
  active_bridge_->OnMessageFromSink(message);
}

void CastRemotingConnector::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  if (deprecated_remoter_)
    deprecated_remoter_->EstimateTransmissionCapacity(std::move(callback));
  else
    std::move(callback).Run(0);
}

void CastRemotingConnector::OnSinkAvailable(
    media::mojom::RemotingSinkMetadataPtr metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;

  // The receiver's metadata should be unchanged during an active remoting
  // session.
  if (active_bridge_) {
    LOG(WARNING) << "Unexpected OnSinkAvailable() call during an active"
                 << "remoting session.";
    return;
  }
  sink_metadata_ = *metadata;
#if !defined(OS_ANDROID)
  sink_metadata_.features.push_back(
      media::mojom::RemotingSinkFeature::RENDERING);
#endif

  if (remoting_allowed_.value_or(true)) {
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkAvailable(sink_metadata_);
  }
}

void CastRemotingConnector::OnError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::UNEXPECTED_FAILURE, false);
}

void CastRemotingConnector::OnSinkGone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;
  sink_metadata_ = RemotingSinkMetadata();
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::SERVICE_GONE, false);
  for (RemotingBridge* notifyee : bridges_)
    notifyee->OnSinkGone();
}

void CastRemotingConnector::OnStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(remoter_);
  if (active_bridge_) {
    active_bridge_->OnStarted();
  } else {
    remoter_->Stop(RemotingStopReason::SOURCE_GONE);
  }
}

void CastRemotingConnector::OnStartFailed(RemotingStartFailReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (active_bridge_)
    active_bridge_->OnStartFailed(reason);
}

void CastRemotingConnector::OnDataSendFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A single data send failure is treated as fatal to an active remoting
  // session.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::DATA_SEND_FAILED, false);
}

void CastRemotingConnector::StartObservingPref() {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kMediaRouterMediaRemotingEnabled,
      base::BindRepeating(&CastRemotingConnector::OnPrefChanged,
                          base::Unretained(this)));
}

void CastRemotingConnector::OnPrefChanged() {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kMediaRouterMediaRemotingEnabled);
  bool enabled = false;
  pref->GetValue()->GetAsBoolean(&enabled);
  remoting_allowed_ = enabled;
  if (!enabled)
    OnStopped(media::mojom::RemotingStopReason::USER_DISABLED);
}
