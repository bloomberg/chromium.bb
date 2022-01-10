// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/login/oobe_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"  // nogncheck
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class WebUIDataSource;
}

namespace chromeos {
class NetworkStateInformer;
class OobeDisplayChooser;
class SigninScreenHandler;
class SigninScreenHandlerDelegate;

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public ui::MojoWebUIController {
 public:
  // List of known types of OobeUI. Type added as path in chrome://oobe url, for
  // example chrome://oobe/login.
  static const char kAppLaunchSplashDisplay[];
  static const char kGaiaSigninDisplay[];
  static const char kLockDisplay[];
  static const char kLoginDisplay[];
  static const char kOobeDisplay[];

  class Observer {
   public:
    Observer() {}

    Observer(const Observer&) = delete;

    virtual void OnCurrentScreenChanged(OobeScreenId current_screen,
                                        OobeScreenId new_screen) = 0;

    virtual void OnDestroyingOobeUI() = 0;

   protected:
    virtual ~Observer() {}
  };

  OobeUI(content::WebUI* web_ui, const GURL& url);

  OobeUI(const OobeUI&) = delete;
  OobeUI& operator=(const OobeUI&) = delete;

  ~OobeUI() override;

  CoreOobeView* GetCoreOobeView();
  ErrorScreen* GetErrorScreen();

  // Collects localized strings from the owned handlers.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Initializes the handlers.
  void InitializeHandlers();

  // Called when the screen has changed.
  void CurrentScreenChanged(OobeScreenId screen);

  bool IsScreenInitialized(OobeScreenId screen);

  bool IsJSReady(base::OnceClosure display_is_ready_callback);

  // Shows or hides OOBE UI elements.
  void ShowOobeUI(bool show);

  // Shows the signin screen.
  void ShowSigninScreen(SigninScreenHandlerDelegate* delegate);

  // Forwards an accelerator to the webui to be handled.
  void ForwardAccelerator(std::string accelerator_name);

  gfx::NativeView GetNativeView();

  gfx::NativeWindow GetTopLevelNativeWindow();

  gfx::Size GetViewSize();

  // Add and remove observers for screen change events.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  OobeScreenId current_screen() const { return current_screen_; }

  OobeScreenId previous_screen() const { return previous_screen_; }

  const std::string& display_type() const { return display_type_; }

  SigninScreenHandler* signin_screen_handler() {
    return signin_screen_handler_;
  }

  NetworkStateInformer* network_state_informer_for_test() const {
    return network_state_informer_.get();
  }

  // Re-evaluate OOBE display placement.
  void OnDisplayConfigurationChanged();

  // Notify WebUI of the user count on the views login screen.
  void SetLoginUserCount(int user_count);

  void OnSystemTrayBubbleShown();

  // Find a *View instance provided by a given *Handler type.
  //
  // This is the same as GetHandler() except the return type is limited to the
  // view.
  template <typename THandler>
  typename THandler::TView* GetView() {
    return GetHandler<THandler>();
  }

  // Find a handler instance.
  template <typename THandler>
  THandler* GetHandler() {
    OobeScreenId expected_screen = THandler::kScreenId;
    for (BaseScreenHandler* handler : screen_handlers_) {
      if (expected_screen == handler->oobe_screen())
        return static_cast<THandler*>(handler);
    }

    NOTREACHED() << "Unable to find handler for screen " << expected_screen;
    return nullptr;
  }

  // Instantiates implementor of the mojom::MultiDeviceSetup mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
          receiver);
  // Instantiates implementor of the mojom::PrivilegedHostDeviceSetter mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          multidevice_setup::mojom::PrivilegedHostDeviceSetter> receiver);
  // Instantiates implementor of the mojom::CrosNetworkConfig mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

  // Instantiates implementor of the mojom::ESimManager mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver);

  static void AddOobeComponents(content::WebUIDataSource* source,
                                const base::DictionaryValue& localized_strings);

  bool ready() const { return ready_; }

 private:
  void AddWebUIHandler(std::unique_ptr<BaseWebUIHandler> handler);
  void AddScreenHandler(std::unique_ptr<BaseScreenHandler> handler);

  // Configures all the relevant screen shandlers and resources for OOBE/Login
  // display type.
  void ConfigureOobeDisplay();

  // Type of UI.
  std::string display_type_;

  // Reference to NetworkStateInformer that handles changes in network
  // state.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Reference to CoreOobeHandler that handles common requests of Oobe page.
  CoreOobeHandler* core_handler_ = nullptr;

  // Reference to SigninScreenHandler that handles sign-in screen requests and
  // forwards calls from native code to JS side.
  SigninScreenHandler* signin_screen_handler_ = nullptr;

  std::vector<BaseWebUIHandler*> webui_handlers_;       // Non-owning pointers.
  std::vector<BaseWebUIHandler*> webui_only_handlers_;  // Non-owning pointers.
  std::vector<BaseScreenHandler*> screen_handlers_;     // Non-owning pointers.

  std::unique_ptr<ErrorScreen> error_screen_;

  // Id of the current oobe/login screen.
  OobeScreenId current_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // Id of the previous oobe/login screen.
  OobeScreenId previous_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // Flag that indicates whether JS part is fully loaded and ready to accept
  // calls.
  bool ready_ = false;

  // Callbacks to notify when JS part is fully loaded and ready to accept calls.
  base::OnceClosureList ready_callbacks_;

  // List of registered observers.
  base::ObserverList<Observer>::Unchecked observer_list_;

  std::unique_ptr<OobeDisplayChooser> oobe_display_chooser_;

  // Store the deferred JS calls before the screen handler instance is
  // initialized.
  std::unique_ptr<JSCallsContainer> js_calls_container_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::OobeUI;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
