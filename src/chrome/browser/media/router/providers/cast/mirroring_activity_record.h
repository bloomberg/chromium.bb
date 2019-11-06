// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/providers/cast/activity_record.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojo/media_router.mojom-forward.h"
#include "components/cast_channel/cast_message_handler.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media_router {

struct CastSinkExtraData;

class MirroringActivityRecord : public ActivityRecord,
                                public mirroring::mojom::SessionObserver,
                                public mirroring::mojom::CastMessageChannel {
 public:
  using OnStopCallback = base::OnceCallback<void()>;

  MirroringActivityRecord(const MediaRoute& route,
                          const std::string& app_id,
                          cast_channel::CastMessageHandler* message_handler,
                          CastSessionTracker* session_tracker,
                          DataDecoder* data_decoder,
                          int target_tab_id,
                          const CastSinkExtraData& cast_data,
                          mojom::MediaRouter* media_router,
                          OnStopCallback callback);
  ~MirroringActivityRecord() override;

  // SessionObserver implementation
  void OnError(mirroring::mojom::SessionError error) override;
  void DidStart() override;
  void DidStop() override;

  // CastMessageChannel implementation
  void Send(mirroring::mojom::CastMessagePtr message) override;

  // ActivityRecord implementation
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) override;
  base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) override;
  void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) override;
  void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      const std::string& hash_token,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) override;
  void HandleLeaveSession(const std::string& client_id) override;
  mojom::RoutePresentationConnectionPtr AddClient(const CastMediaSource& source,
                                                  const url::Origin& origin,
                                                  int tab_id) override;
  void RemoveClient(const std::string& client_id) override;
  void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) override;
  void SendMediaStatusToClients(const base::Value& media_status,
                                base::Optional<int> request_id) override;
  void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason) override;
  void TerminatePresentationConnections() override;
  void OnAppMessage(const cast_channel::CastMessage& message) override;

 private:
  enum class MirroringType {
    kTab,           // Mirror a single tab.
    kDesktop,       // Mirror the whole desktop.
    kOffscreenTab,  // Used for Presentation API 1UA mode.
    kMaxValue = kOffscreenTab,
  };

  void StopMirroring();

  mirroring::mojom::MirroringServiceHostPtr host_;

  // Sends Cast messages from the mirroring receiver to the mirroring service.
  mirroring::mojom::CastMessageChannelPtr channel_to_service_;

  mojo::Binding<mirroring::mojom::SessionObserver> observer_binding_{this};

  // To handle Cast messages from the mirroring service to the mirroring
  // receiver.
  mojo::Binding<mirroring::mojom::CastMessageChannel> channel_binding_{this};

  const int channel_id_;
  const MirroringType mirroring_type_;
  OnStopCallback on_stop_;
  base::WeakPtrFactory<MirroringActivityRecord> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_
