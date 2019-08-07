// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/pip/arc_picture_in_picture_window_controller_impl.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/picture_in_picture_window_controller.h"

namespace arc {

namespace {

// Singleton factory for ArcPipBridge.
class ArcPipBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPipBridge,
          ArcPipBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPipBridgeFactory";

  static ArcPipBridgeFactory* GetInstance() {
    return base::Singleton<ArcPipBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPipBridgeFactory>;

  ArcPipBridgeFactory() = default;
  ~ArcPipBridgeFactory() override = default;
};

}  // namespace

// static
ArcPipBridge* ArcPipBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPipBridgeFactory::GetForBrowserContext(context);
}

ArcPipBridge::ArcPipBridge(content::BrowserContext* context,
                           ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      profile_(Profile::FromBrowserContext(context)) {
  DCHECK(context);
  DCHECK(profile_);

  DVLOG(2) << "ArcPipBridge::ArcPipBridge";
  arc_bridge_service_->pip()->SetHost(this);
  arc_bridge_service_->pip()->AddObserver(this);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(&ArcPipBridge::OnSpokenFeedbackChanged,
                          base::Unretained(this)));
}

ArcPipBridge::~ArcPipBridge() {
  DVLOG(2) << "ArcPipBridge::~ArcPipBridge";
  arc_bridge_service_->pip()->RemoveObserver(this);
  arc_bridge_service_->pip()->SetHost(nullptr);
}

void ArcPipBridge::OnConnectionReady() {
  DVLOG(1) << "ArcPipBridge::OnConnectionReady";
  // Send the initial status.
  OnSpokenFeedbackChanged();
}

void ArcPipBridge::OnConnectionClosed() {
  DVLOG(1) << "ArcPipBridge::OnConnectionClosed";
}

void ArcPipBridge::OnPipEvent(arc::mojom::ArcPipEvent event) {
  DVLOG(1) << "ArcPipBridge::OnPipEvent";

  switch (event) {
    case mojom::ArcPipEvent::ENTER: {
      auto pip_window_controller =
          std::make_unique<ArcPictureInPictureWindowControllerImpl>(this);
      // Make sure not to close PIP if we are replacing an existing Android PIP.
      base::AutoReset<bool> auto_prevent_closing_pip(&prevent_closing_pip_,
                                                     true);
      PictureInPictureWindowManager::GetInstance()
          ->EnterPictureInPictureWithController(pip_window_controller.get());
      pip_window_controller_ = std::move(pip_window_controller);
      break;
    }
  }
}

void ArcPipBridge::ClosePip() {
  DVLOG(1) << "ArcPipBridge::ClosePip";

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->pip(), ClosePip);
  if (!instance)
    return;

  if (!prevent_closing_pip_)
    instance->ClosePip();
}

bool ArcPipBridge::ShouldSuppressPip() const {
  return profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);
}

void ArcPipBridge::OnSpokenFeedbackChanged() {
  const bool should_suppress = ShouldSuppressPip();

  DVLOG(1) << "ArcPipBridge::OnSpokenFeedbackChanged (status: "
           << should_suppress << ")";

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->pip(),
                                               SetPipSuppressionStatus);
  if (!instance)
    return;

  instance->SetPipSuppressionStatus(should_suppress);

  // TODO(yoshiki): Add the code to suppress Chrome PIP window when the spoken
  // feedback gets enabled.
}

}  // namespace arc
