// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace arc {
namespace {

// Singleton factory for ArcInputOverlayManager.
class ArcInputOverlayManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputOverlayManager,
          ArcInputOverlayManagerFactory> {
 public:
  static constexpr const char* kName = "ArcInputOverlayManagerFactory";

  static ArcInputOverlayManagerFactory* GetInstance() {
    return base::Singleton<ArcInputOverlayManagerFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcInputOverlayManagerFactory>;
  ArcInputOverlayManagerFactory() = default;
  ~ArcInputOverlayManagerFactory() override = default;
};

}  // namespace

class ArcInputOverlayManager::InputMethodObserver
    : public ui::InputMethodObserver {
 public:
  explicit InputMethodObserver(ArcInputOverlayManager* owner) : owner_(owner) {}
  InputMethodObserver(const InputMethodObserver&) = delete;
  InputMethodObserver& operator=(const InputMethodObserver&) = delete;
  ~InputMethodObserver() override = default;

  // ui::InputMethodObserver overrides:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {
    owner_->is_text_input_active_ =
        client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE &&
        client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NULL;
    owner_->NotifyTextInputState();
  }
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {
    owner_->input_method_ = nullptr;
  }

 private:
  ArcInputOverlayManager* const owner_;
};

// static
ArcInputOverlayManager* ArcInputOverlayManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputOverlayManagerFactory::GetForBrowserContext(context);
}

ArcInputOverlayManager::ArcInputOverlayManager(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : input_method_observer_(std::make_unique<InputMethodObserver>(this)) {
  if (aura::Env::HasInstance())
    env_observation_.Observe(aura::Env::GetInstance());
  if (ash::Shell::HasInstance() && ash::Shell::GetPrimaryRootWindow()) {
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
  }

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       // Should not block shutdown.
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // For test. The unittest is based on ExoTestBase which must run on
  // Chrome_UIThread. While TestingProfileManager::CreateTestingProfile runs on
  // MainThread.
  if (browser_context)
    data_controller_ = std::make_unique<input_overlay::DataController>(
        *browser_context, task_runner_);
}

ArcInputOverlayManager::~ArcInputOverlayManager() = default;

void ArcInputOverlayManager::ReadData(const std::string& package_name,
                                      aura::Window* top_level_window) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcInputOverlayManager::ReadDefaultData, Unretained(this),
                     package_name, top_level_window),
      base::BindOnce(&ArcInputOverlayManager::ReadCustomizedData,
                     Unretained(this), package_name));
}

input_overlay::TouchInjector* ArcInputOverlayManager::ReadDefaultData(
    const std::string& package_name,
    aura::Window* top_level_window) {
  absl::optional<int> resource_id = GetInputOverlayResourceId(package_name);
  if (!resource_id)
    return nullptr;
  const base::StringPiece json_file =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource_id.value());
  if (json_file.empty()) {
    LOG(WARNING) << "No content for: " << package_name;
    return nullptr;
  }
  base::JSONReader::ValueWithError result =
      base::JSONReader::ReadAndReturnValueWithError(json_file);
  DCHECK(result.value) << "Could not load input overlay data file: "
                       << result.error_message;
  if (!result.value)
    return nullptr;

  base::Value& root = result.value.value();
  std::unique_ptr<input_overlay::TouchInjector> injector =
      std::make_unique<input_overlay::TouchInjector>(
          top_level_window,
          base::BindRepeating(&ArcInputOverlayManager::OnSaveProtoFile,
                              weak_ptr_factory_.GetWeakPtr()));
  injector->ParseActions(root);
  auto res = input_overlay_enabled_windows_.emplace(top_level_window,
                                                    std::move(injector));
  DCHECK(res.second);
  return res.first->second.get();
}

void ArcInputOverlayManager::ReadCustomizedData(
    const std::string& package_name,
    input_overlay::TouchInjector* touch_injector) {
  if (!touch_injector)
    return;

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcInputOverlayManager::GetProto, Unretained(this),
                     package_name),
      base::BindOnce(&ArcInputOverlayManager::OnProtoDataAvailable,
                     Unretained(this), touch_injector));
}

std::unique_ptr<input_overlay::AppDataProto> ArcInputOverlayManager::GetProto(
    const std::string& package_name) {
  // |data_controller_| is null for test.
  return data_controller_ ? data_controller_->ReadProtoFromFile(package_name)
                          : nullptr;
}

void ArcInputOverlayManager::OnProtoDataAvailable(
    input_overlay::TouchInjector* touch_injector,
    std::unique_ptr<input_overlay::AppDataProto> proto) {
  if (proto) {
    touch_injector->OnProtoDataAvailable(*proto);
  } else {
    touch_injector->set_first_launch(true);
  }

  touch_injector->set_data_reading_finished(true);
  RegisterWindowIfFocused(touch_injector->target_window());
}

void ArcInputOverlayManager::OnSaveProtoFile(
    std::unique_ptr<input_overlay::AppDataProto> proto,
    const std::string& package_name) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArcInputOverlayManager::SaveFile, base::Unretained(this),
                     std::move(proto), package_name));
}

void ArcInputOverlayManager::SaveFile(
    std::unique_ptr<input_overlay::AppDataProto> proto,
    const std::string& package_name) {
  if (data_controller_)
    data_controller_->WriteProtoToFile(std::move(proto), package_name);
}

void ArcInputOverlayManager::NotifyTextInputState() {
  auto it = input_overlay_enabled_windows_.find(registered_top_level_window_);
  if (it != input_overlay_enabled_windows_.end())
    it->second->NotifyTextInputState(is_text_input_active_);
}

void ArcInputOverlayManager::AddObserverToInputMethod() {
  if (!registered_top_level_window_)
    return;
  DCHECK(registered_top_level_window_->GetHost());
  DCHECK(!input_method_);
  input_method_ = registered_top_level_window_->GetHost()->GetInputMethod();
  if (input_method_)
    input_method_->AddObserver(input_method_observer_.get());
}

void ArcInputOverlayManager::RemoveObserverFromInputMethod() {
  if (!input_method_)
    return;
  input_method_->RemoveObserver(input_method_observer_.get());
  input_method_ = nullptr;
}

void ArcInputOverlayManager::RegisterWindow(aura::Window* window) {
  if (!window || window != window->GetToplevelWindow() ||
      registered_top_level_window_ == window) {
    return;
  }
  auto it = input_overlay_enabled_windows_.find(window);
  if (it == input_overlay_enabled_windows_.end())
    return;
  DCHECK(!registered_top_level_window_);
  // Don't register window if the data reading is not finished. It still will be
  // registered after data reading finished by |RegisterWindowIfFocused|.
  if (!it->second->data_reading_finished())
    return;

  it->second->RegisterEventRewriter();
  registered_top_level_window_ = window;
  AddObserverToInputMethod();
  AddDisplayOverlayController(it->second.get());
  // If the window is on the extended window, it turns out only primary root
  // window catches the key event. So it needs to forward the key event from
  // primary root window to extended root window event source.
  if (registered_top_level_window_->GetRootWindow() !=
      ash::Shell::GetPrimaryRootWindow()) {
    key_event_source_rewriter_ =
        std::make_unique<KeyEventSourceRewriter>(registered_top_level_window_);
  }
}

void ArcInputOverlayManager::UnRegisterWindow(aura::Window* window) {
  if (!registered_top_level_window_ || registered_top_level_window_ != window)
    return;
  auto it = input_overlay_enabled_windows_.find(registered_top_level_window_);
  DCHECK(it != input_overlay_enabled_windows_.end());
  if (it == input_overlay_enabled_windows_.end())
    return;
  if (key_event_source_rewriter_)
    key_event_source_rewriter_.reset();
  it->second->UnRegisterEventRewriter();
  RemoveDisplayOverlayController();
  RemoveObserverFromInputMethod();
  registered_top_level_window_ = nullptr;
}

void ArcInputOverlayManager::RegisterWindowIfFocused(aura::Window* window) {
  if (ash::window_util::GetFocusedWindow() == window)
    RegisterWindow(window);
}

void ArcInputOverlayManager::AddDisplayOverlayController(
    input_overlay::TouchInjector* touch_injector) {
  DCHECK(registered_top_level_window_);
  DCHECK(touch_injector);
  if (!registered_top_level_window_ || !touch_injector)
    return;
  DCHECK(!display_overlay_controller_);

  display_overlay_controller_ =
      std::make_unique<input_overlay::DisplayOverlayController>(
          touch_injector, touch_injector->first_launch());
}

void ArcInputOverlayManager::RemoveDisplayOverlayController() {
  if (!registered_top_level_window_)
    return;
  DCHECK(display_overlay_controller_);
  display_overlay_controller_.reset();
}

void ArcInputOverlayManager::OnWindowInitialized(aura::Window* new_window) {
  if (window_observations_.IsObservingSource(new_window))
    return;

  window_observations_.AddObservation(new_window);
}

void ArcInputOverlayManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (!window || key != ash::kArcPackageNameKey)
    return;

  auto* top_level_window = window->GetToplevelWindow();
  if (top_level_window &&
      !input_overlay_enabled_windows_.contains(top_level_window)) {
    auto* package_name = window->GetProperty(ash::kArcPackageNameKey);
    if (!package_name || package_name->empty())
      return;
    ReadData(*package_name, top_level_window);
  }
}

void ArcInputOverlayManager::OnWindowDestroying(aura::Window* window) {
  if (window_observations_.IsObservingSource(window))
    window_observations_.RemoveObservation(window);
  UnRegisterWindow(window);
  input_overlay_enabled_windows_.erase(window);
}

void ArcInputOverlayManager::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window)
    return;
  RegisterWindow(window);
}

void ArcInputOverlayManager::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  if (!window)
    return;
  // There might be child window surface removing, we don't unregister window
  // until the top_level_window is removed from the root.
  UnRegisterWindow(window);
}

void ArcInputOverlayManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!window || window != registered_top_level_window_)
    return;
  if (display_overlay_controller_)
    display_overlay_controller_->OnWindowBoundsChanged();
}

void ArcInputOverlayManager::Shutdown() {
  UnRegisterWindow(registered_top_level_window_);
  window_observations_.RemoveAllObservations();
  env_observation_.Reset();
  if (ash::Shell::HasInstance() && ash::Shell::GetPrimaryRootWindow()) {
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
  }
}

void ArcInputOverlayManager::OnWindowFocused(aura::Window* gained_focus,
                                             aura::Window* lost_focus) {
  aura::Window* lost_focus_top_level_window = nullptr;
  aura::Window* gained_focus_top_level_window = nullptr;

  if (lost_focus)
    lost_focus_top_level_window = lost_focus->GetToplevelWindow();

  if (gained_focus)
    gained_focus_top_level_window = gained_focus->GetToplevelWindow();

  if (lost_focus_top_level_window == gained_focus_top_level_window)
    return;

  UnRegisterWindow(lost_focus_top_level_window);
  RegisterWindow(gained_focus_top_level_window);
}

}  // namespace arc
