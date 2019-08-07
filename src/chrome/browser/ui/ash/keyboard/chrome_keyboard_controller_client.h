// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_

#include <memory>
#include <set>
#include <vector>

#include "ash/public/interfaces/keyboard_config.mojom.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "url/gurl.h"

class ChromeKeyboardWebContents;
class Profile;

namespace aura {
class Window;
}

namespace service_manager {
class Connector;
}

// This class implements mojom::KeyboardControllerObserver and makes calls
// into the mojom::KeyboardController service. It also observes keyboard prefs
// and enables or disables the keyboard accordingly.
class ChromeKeyboardControllerClient
    : public ash::mojom::KeyboardControllerObserver,
      public session_manager::SessionManagerObserver {
 public:
  // Convenience observer allowing UI classes to observe the global instance of
  // this class instead of setting up mojo bindings.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Forwards the 'OnKeyboardVisibilityChanged' mojo observer method.
    // This is used by oobe and login to adjust the UI.
    virtual void OnKeyboardVisibilityChanged(bool visible) {}

    // Forwards the 'OnKeyboardOccludedBoundsChanged' mojo observer method.
    // This is used to update the insets of browser and app windows when the
    // keyboard is shown.
    virtual void OnKeyboardOccludedBoundsChanged(
        const gfx::Rect& screen_bounds) {}

    // Notifies observers when the keyboard content (i.e. the extension) has
    // loaded. Note: if the content is already loaded when the observer is
    // added, this will not be triggered, but see is_keyboard_loaded().
    virtual void OnKeyboardLoaded() {}
  };

  // Creates the singleton instance for chrome or browser tests.
  static std::unique_ptr<ChromeKeyboardControllerClient> Create(
      service_manager::Connector* connector);

  // Creates the singleton instance for unit tests where there is no
  // SessionManager. Prefs will not be observed or affect the enabled state.
  static std::unique_ptr<ChromeKeyboardControllerClient> CreateForTest(
      service_manager::Connector* connector);

  // Static getter. The single instance must be instantiated first.
  static ChromeKeyboardControllerClient* Get();

  // Used in tests to determine whether this has been instantiated.
  static bool HasInstance();

  ~ChromeKeyboardControllerClient() override;

  // Called before Shell or the primary profile is destroyed.
  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // In Classic Ash, notifies this that the contents have loaded, triggering
  // OnKeyboardLoaded.
  void NotifyKeyboardLoaded();

  // Returns the cached KeyboardConfig value.
  keyboard::mojom::KeyboardConfig GetKeyboardConfig();

  // Sets the new keyboard configuration and updates the cached config.
  void SetKeyboardConfig(const keyboard::mojom::KeyboardConfig& config);

  // Invokes |callback| with the current enabled state. Call this after
  // Set/ClearEnableFlag to get the updated enabled state.
  void GetKeyboardEnabled(base::OnceCallback<void(bool)> callback);

  // Sets/clears the privided keyboard enable state.
  void SetEnableFlag(const keyboard::mojom::KeyboardEnableFlag& flag);
  void ClearEnableFlag(const keyboard::mojom::KeyboardEnableFlag& flag);

  // Returns whether |flag| has been set.
  bool IsEnableFlagSet(const keyboard::mojom::KeyboardEnableFlag& flag);

  // Calls forwarded to ash.mojom.KeyboardController.
  void ReloadKeyboardIfNeeded();
  void RebuildKeyboardIfEnabled();
  void ShowKeyboard();
  void HideKeyboard(ash::mojom::HideReason reason);
  void SetContainerType(keyboard::mojom::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds,
                        base::OnceCallback<void(bool)> callback);
  void SetKeyboardLocked(bool locked);
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds);
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds);
  void SetDraggableArea(const gfx::Rect& bounds);

  // Returns true if overscroll is enabled by the config or command line.
  bool IsKeyboardOverscrollEnabled();

  // Returns the URL to use for the virtual keyboard.
  GURL GetVirtualKeyboardUrl();

  // Returns the keyboard window, or null if the window has not been created.
  aura::Window* GetKeyboardWindow() const;

  bool is_keyboard_enabled() { return is_keyboard_enabled_; }
  bool is_keyboard_loaded() { return is_keyboard_loaded_; }
  bool is_keyboard_visible() { return is_keyboard_visible_; }

  void FlushForTesting();

  void set_profile_for_test(Profile* profile) { profile_for_test_ = profile; }
  void set_virtual_keyboard_url_for_test(const GURL& url) {
    virtual_keyboard_url_for_test_ = url;
  }

 private:
  // The constructor is private to ensure that SessionManager exists for non
  // test usage. Use Create or CreateForTest instead.
  explicit ChromeKeyboardControllerClient(
      service_manager::Connector* connector);

  // Called from Create() to observer session_SessionManager and initialize
  // |pref_change_registrar_| once the session starts.
  void InitializePrefObserver();

  // keyboard::mojom::KeyboardControllerObserver:
  void OnKeyboardEnableFlagsChanged(
      const std::vector<keyboard::mojom::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool enabled) override;
  void OnKeyboardConfigChanged(
      keyboard::mojom::KeyboardConfigPtr config) override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnLoadKeyboardContentsRequested() override;
  void OnKeyboardUIDestroyed() override;

  void OnKeyboardContentsLoaded(const gfx::Size& size);

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Sets whether the virtual keyboard is enabled from prefs.
  void SetVirtualKeyboardBehaviorFromPrefs();

  // Returns either the test profile or the active user profile.
  Profile* GetProfile();

  gfx::Rect BoundsFromScreen(const gfx::Rect& screen_bounds);

  PrefChangeRegistrar pref_change_registrar_;

  ash::mojom::KeyboardControllerPtr keyboard_controller_ptr_;
  mojo::AssociatedBinding<ash::mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  // Set when the WS is used and OnLoadKeyboardContentsRequested is called.
  std::unique_ptr<ChromeKeyboardWebContents> keyboard_contents_;

  // Cached copy of the latest config provided by mojom::KeyboardController.
  keyboard::mojom::KeyboardConfigPtr cached_keyboard_config_;

  // Cached copy of the active enabled flags provided by
  // mojom::KeyboardController
  std::set<keyboard::mojom::KeyboardEnableFlag> keyboard_enable_flags_;

  // Tracks the enabled state of the keyboard.
  bool is_keyboard_enabled_ = false;

  // Tracks when the keyboard content has loaded.
  bool is_keyboard_loaded_ = false;

  // Tracks the visible state of the keyboard.
  bool is_keyboard_visible_ = false;

  base::ObserverList<Observer> observers_;

  Profile* profile_for_test_ = nullptr;
  GURL virtual_keyboard_url_for_test_;

  base::WeakPtrFactory<ChromeKeyboardControllerClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
