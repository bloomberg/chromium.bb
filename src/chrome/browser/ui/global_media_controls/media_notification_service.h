// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "media/audio/audio_device_description.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class StartPresentationContext;
class WebContents;
}  // namespace content

namespace global_media_controls {
class MediaDialogDelegate;
class MediaItemManager;
}  // namespace global_media_controls

namespace media_router {
class CastDialogController;
}  // namespace media_router

class MediaSessionNotificationProducer;

class MediaNotificationService : public KeyedService {
 public:
  MediaNotificationService(Profile* profile, bool show_from_all_profiles);
  MediaNotificationService(const MediaNotificationService&) = delete;
  MediaNotificationService& operator=(const MediaNotificationService&) = delete;
  ~MediaNotificationService() override;

  // KeyedService implementation.
  void Shutdown() override;

  global_media_controls::MediaItemManager* media_item_manager() {
    return item_manager_.get();
  }

  void SetDialogDelegateForWebContents(
      global_media_controls::MediaDialogDelegate* delegate,
      content::WebContents* contents);

  // True if there are active non-frozen media session notifications or active
  // cast notifications associated with |web_contents|.
  bool HasActiveNotificationsForWebContents(
      content::WebContents* web_contents) const;

  // True if there are local cast notifications.
  bool HasLocalCastNotifications() const;

  // Used by a |MediaNotificationDeviceSelectorView| to query the system
  // for connected audio output devices.
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallback callback);

  // Used by a |MediaNotificationAudioDeviceSelectorView| to become notified of
  // audio device switching capabilities. The callback will be immediately run
  // with the current availability.
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback);

  void OnStartPresentationContextCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);

  // Instantiates a MediaRouterViewsUI object associated with the Session with
  // the given |session_id|.
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForSession(const std::string& session_id);

  // Instantiates a MediaRouterViewsUI object associated with the
  // PresentationRequest that |presentation_request_notification_producer_|
  // manages.
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForPresentationRequest();

 private:
  friend class MediaNotificationProviderImplTest;
  friend class MediaNotificationServiceTest;
  friend class MediaNotificationServiceCastTest;
  friend class MediaToolbarButtonControllerTest;
  friend class PresentationRequestNotificationProducerTest;

  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HideAfterTimeoutAndActiveAgainOnPlay);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           SessionIsRemovedImmediatelyWhenATabCloses);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest, DismissesMediaSession);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidesInactiveNotifications);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidingNotification_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceCastTest,
                           ShowSupplementalNotifications);

  // True if there are cast notifications associated with |web_contents|.
  bool HasCastNotificationsForWebContents(
      content::WebContents* web_contents) const;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<MediaSessionNotificationProducer>
      media_session_notification_producer_;
  std::unique_ptr<CastMediaNotificationProducer> cast_notification_producer_;
  std::unique_ptr<PresentationRequestNotificationProducer>
      presentation_request_notification_producer_;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
