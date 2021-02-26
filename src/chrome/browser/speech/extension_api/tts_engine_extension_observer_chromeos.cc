// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"

#include "base/check.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/service_sandbox_type.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

void UpdateGoogleSpeechSynthesisKeepAliveCountHelper(
    content::BrowserContext* context,
    bool increment) {
  extensions::ProcessManager* pm = extensions::ProcessManager::Get(context);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(
          extension_misc::kGoogleSpeechSynthesisExtensionId);
  if (!extension)
    return;

  if (increment) {
    pm->IncrementLazyKeepaliveCount(
        extension, extensions::Activity::ACCESSIBILITY, std::string());
  } else {
    pm->DecrementLazyKeepaliveCount(
        extension, extensions::Activity::ACCESSIBILITY, std::string());
  }
}

void UpdateGoogleSpeechSynthesisKeepAliveCount(content::BrowserContext* context,
                                               bool increment) {
  // Deal with profiles that are non-off the record and otr. For a given
  // extension load/unload, we only ever get called for one of the two potential
  // profile types.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return;

  UpdateGoogleSpeechSynthesisKeepAliveCountHelper(
      profile->HasPrimaryOTRProfile() ? profile->GetPrimaryOTRProfile()
                                      : profile,
      increment);
}

void UpdateGoogleSpeechSynthesisKeepAliveCountOnReload(
    content::BrowserContext* browser_context) {
  if (chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
    UpdateGoogleSpeechSynthesisKeepAliveCount(browser_context,
                                              true /* increment */);
  }

  if (chromeos::AccessibilityManager::Get()->IsSelectToSpeakEnabled()) {
    UpdateGoogleSpeechSynthesisKeepAliveCount(browser_context,
                                              true /* increment */);
  }
}

}  // namespace

// Factory to load one instance of TtsExtensionLoaderChromeOs per profile.
class TtsEngineExtensionObserverChromeOSFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static TtsEngineExtensionObserverChromeOS* GetForProfile(Profile* profile) {
    return static_cast<TtsEngineExtensionObserverChromeOS*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static TtsEngineExtensionObserverChromeOSFactory* GetInstance() {
    return base::Singleton<TtsEngineExtensionObserverChromeOSFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      TtsEngineExtensionObserverChromeOSFactory>;

  TtsEngineExtensionObserverChromeOSFactory()
      : BrowserContextKeyedServiceFactory(
            "TtsEngineExtensionObserverChromeOS",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(extensions::EventRouterFactory::GetInstance());
  }

  ~TtsEngineExtensionObserverChromeOSFactory() override {}

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    // If given an incognito profile (including the Chrome OS login
    // profile), share the service with the original profile.
    return chrome::GetBrowserContextRedirectedInIncognito(context);
  }

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new TtsEngineExtensionObserverChromeOS(
        static_cast<Profile*>(profile));
  }
};

TtsEngineExtensionObserverChromeOS*
TtsEngineExtensionObserverChromeOS::GetInstance(Profile* profile) {
  return TtsEngineExtensionObserverChromeOSFactory::GetInstance()
      ->GetForProfile(profile);
}

TtsEngineExtensionObserverChromeOS::TtsEngineExtensionObserverChromeOS(
    Profile* profile)
    : extension_registry_observer_(this), profile_(profile) {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  DCHECK(event_router);
  event_router->RegisterObserver(this, tts_engine_events::kOnSpeak);
  event_router->RegisterObserver(this, tts_engine_events::kOnStop);

  accessibility_status_subscription_ =
      chromeos::AccessibilityManager::Get()->RegisterCallback(
          base::BindRepeating(
              &TtsEngineExtensionObserverChromeOS::OnAccessibilityStatusChanged,
              base::Unretained(this)));
}

TtsEngineExtensionObserverChromeOS::~TtsEngineExtensionObserverChromeOS() =
    default;

void TtsEngineExtensionObserverChromeOS::BindTtsStream(
    mojo::PendingReceiver<chromeos::tts::mojom::TtsStream> receiver) {
  // At this point, the component extension has loaded, and the js has requested
  // a TtsStream be bound. It's safe now to update the keep alive count for
  // important accessibility features. This path is also encountered if the
  // component extension background page forceably window.close(s) on error.
  UpdateGoogleSpeechSynthesisKeepAliveCountOnReload(profile_);

  // Always launch a new TtsService. By assigning below, if |tts_service_| held
  // a remote, it will be killed and a new one created, ensuring we only ever
  // have one TtsService running.
  tts_service_ =
      content::ServiceProcessHost::Launch<chromeos::tts::mojom::TtsService>(
          content::ServiceProcessHost::Options()
              .WithDisplayName("TtsService")
              .Pass());

  mojo::PendingRemote<audio::mojom::StreamFactory> factory_remote;
  auto factory_receiver = factory_remote.InitWithNewPipeAndPassReceiver();
  content::GetAudioService().BindStreamFactory(std::move(factory_receiver));
  tts_service_->BindTtsStream(std::move(receiver), std::move(factory_remote));
}

void TtsEngineExtensionObserverChromeOS::Shutdown() {
  extensions::EventRouter::Get(profile_)->UnregisterObserver(this);
}

bool TtsEngineExtensionObserverChromeOS::IsLoadedTtsEngine(
    const std::string& extension_id) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  DCHECK(event_router);
  if (event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnSpeak) &&
      event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnStop)) {
    return true;
  }

  return false;
}

void TtsEngineExtensionObserverChromeOS::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (!IsLoadedTtsEngine(details.extension_id))
    return;

  content::TtsController::GetInstance()->VoicesChanged();
}

void TtsEngineExtensionObserverChromeOS::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kTtsEngine)) {
    engine_extension_ids_.insert(extension->id());

    if (extension->id() == extension_misc::kGoogleSpeechSynthesisExtensionId)
      UpdateGoogleSpeechSynthesisKeepAliveCountOnReload(browser_context);
  }
}

void TtsEngineExtensionObserverChromeOS::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  size_t erase_count = 0;
  erase_count += engine_extension_ids_.erase(extension->id());
  if (erase_count > 0)
    content::TtsController::GetInstance()->VoicesChanged();

  if (tts_service_ &&
      extension->id() == extension_misc::kGoogleSpeechSynthesisExtensionId)
    tts_service_.reset();
}

void TtsEngineExtensionObserverChromeOS::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& details) {
  if (details.notification_type != chromeos::AccessibilityNotificationType::
                                       ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK &&
      details.notification_type != chromeos::AccessibilityNotificationType::
                                       ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK)
    return;

  // Google speech synthesis might not be loaded yet. If it isn't, the call in
  // |OnExtensionLoaded| will do the increment. If it is, the call below will
  // increment. Decrements only occur when toggling off here.
  UpdateGoogleSpeechSynthesisKeepAliveCount(profile(), details.enabled);
}
