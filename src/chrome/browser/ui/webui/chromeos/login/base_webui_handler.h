// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/model_view_channel.h"
#include "components/login/base_screen_handler_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace login {
class LocalizedValuesBuilder;
}

namespace chromeos {

class BaseScreen;
class OobeUI;

// A helper class to store deferred Javascript calls, shared by subclasses of
// BaseWebUIHandler.
// TODO(jdufault): Move into a separate file
class JSCallsContainer {
 public:
  JSCallsContainer();
  ~JSCallsContainer();

  // Used to decide whether the JS call should be deferred.
  bool is_initialized() const { return is_initialized_; }

  // Used to add deferred calls to.
  std::vector<base::Closure>& deferred_js_calls() { return deferred_js_calls_; }

  // Executes Javascript calls that were deferred while the instance was not
  // initialized yet.
  void ExecuteDeferredJSCalls();

 private:
  // Whether the instance is initialized.
  //
  // The instance becomes initialized after the corresponding message is
  // received from Javascript side.
  bool is_initialized_ = false;

  // Javascript calls that have been deferred while the instance was not
  // initialized yet.
  std::vector<base::Closure> deferred_js_calls_;
};

// Base class for all oobe/login WebUI handlers. These handlers are the binding
// layer that allow the C++ and JavaScript code to communicate.
//
// If the deriving type is associated with a specific OobeScreen, it should
// derive from BaseScreenHandler instead of BaseWebUIHandler.
//
// TODO(jdufault): Move all OobeScreen related concepts out of BaseWebUIHandler
// and into BaseScreenHandler.
class BaseWebUIHandler : public content::WebUIMessageHandler,
                         public ModelViewChannel {
 public:
  explicit BaseWebUIHandler(JSCallsContainer* js_calls_container);
  ~BaseWebUIHandler() override;

  // Gets localized strings to be used on the page.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // ModelViewChannel implementation:
  void CommitContextChanges(const base::DictionaryValue& diff) override;

  // This method is called when page is ready. It propagates to inherited class
  // via virtual Initialize() method (see below).
  void InitializeBase();

  // Set the prefix used when running CallJs with a method. For example,
  //    set_call_js_prefix("Oobe")
  //    CallJs("lock") -> Invokes JS global named "Oobe.lock"
  void set_call_js_prefix(const std::string& prefix) {
    js_screen_path_prefix_ = prefix + ".";
  }

  void set_async_assets_load_id(const std::string& async_assets_load_id) {
    async_assets_load_id_ = async_assets_load_id;
  }
  const std::string& async_assets_load_id() const {
    return async_assets_load_id_;
  }

 protected:
  // All subclasses should implement this method to provide localized values.
  virtual void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) = 0;

  // All subclasses should implement this method to register callbacks for JS
  // messages.
  //
  // TODO (ygorshenin, crbug.com/433797): make this method purely vrtual when
  // all screens will be switched to use ScreenContext.
  virtual void DeclareJSCallbacks() {}

  // Subclasses can override these methods to pass additional parameters
  // to loadTimeData. Generally, it is a bad approach, and it should be replaced
  // with Context at some point.
  virtual void GetAdditionalParameters(base::DictionaryValue* parameters);

  void CallJS(const std::string& function_name) {
    if (js_calls_container_->is_initialized()) {
      web_ui()->CallJavascriptFunctionUnsafe(function_name);
    } else {
      js_calls_container_->deferred_js_calls().push_back(
          base::Bind(&BaseWebUIHandler::CallJavascriptFunctionImmediate<>,
                     base::Unretained(this), function_name));
    }
  }

  template <typename A1>
  void CallJS(const std::string& function_name, const A1& arg1) {
    if (js_calls_container_->is_initialized()) {
      web_ui()->CallJavascriptFunctionUnsafe(function_name,
                                             ::login::MakeValue(arg1));
    } else {
      js_calls_container_->deferred_js_calls().push_back(base::Bind(
          &BaseWebUIHandler::CallJavascriptFunctionImmediate<base::Value>,
          base::Unretained(this), function_name,
          ::login::MakeValue(arg1).Clone()));
    }
  }

  template <typename A1, typename A2>
  void CallJS(const std::string& function_name,
              const A1& arg1,
              const A2& arg2) {
    if (js_calls_container_->is_initialized()) {
      web_ui()->CallJavascriptFunctionUnsafe(
          function_name, ::login::MakeValue(arg1), ::login::MakeValue(arg2));
    } else {
      js_calls_container_->deferred_js_calls().push_back(base::Bind(
          &BaseWebUIHandler::CallJavascriptFunctionImmediate<base::Value,
                                                             base::Value>,
          base::Unretained(this), function_name,
          ::login::MakeValue(arg1).Clone(), ::login::MakeValue(arg2).Clone()));
    }
  }

  template <typename A1, typename A2, typename A3>
  void CallJS(const std::string& function_name,
              const A1& arg1,
              const A2& arg2,
              const A3& arg3) {
    if (js_calls_container_->is_initialized()) {
      web_ui()->CallJavascriptFunctionUnsafe(
          function_name, ::login::MakeValue(arg1), ::login::MakeValue(arg2),
          ::login::MakeValue(arg3));
    } else {
      js_calls_container_->deferred_js_calls().push_back(base::Bind(
          &BaseWebUIHandler::CallJavascriptFunctionImmediate<
              base::Value, base::Value, base::Value>,
          base::Unretained(this), function_name,
          ::login::MakeValue(arg1).Clone(), ::login::MakeValue(arg2).Clone(),
          ::login::MakeValue(arg3).Clone()));
    }
  }

  template <typename A1, typename A2, typename A3, typename A4>
  void CallJS(const std::string& function_name,
              const A1& arg1,
              const A2& arg2,
              const A3& arg3,
              const A4& arg4) {
    if (js_calls_container_->is_initialized()) {
      web_ui()->CallJavascriptFunctionUnsafe(
          function_name, ::login::MakeValue(arg1), ::login::MakeValue(arg2),
          ::login::MakeValue(arg3), ::login::MakeValue(arg4));
    } else {
      js_calls_container_->deferred_js_calls().push_back(base::Bind(
          &BaseWebUIHandler::CallJavascriptFunctionImmediate<
              base::Value, base::Value, base::Value, base::Value>,
          base::Unretained(this), function_name,
          ::login::MakeValue(arg1).Clone(), ::login::MakeValue(arg2).Clone(),
          ::login::MakeValue(arg3).Clone(), ::login::MakeValue(arg4).Clone()));
    }
  }

  // Shortcut methods for adding WebUI callbacks.
  template <typename T>
  void AddRawCallback(const std::string& name,
                      void (T::*method)(const base::ListValue* args)) {
    web_ui()->RegisterMessageCallback(
        name,
        base::BindRepeating(method, base::Unretained(static_cast<T*>(this))));
  }

  template <typename T, typename... Args>
  void AddCallback(const std::string& name, void (T::*method)(Args...)) {
    base::RepeatingCallback<void(Args...)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name,
        base::BindRepeating(&::login::CallbackWrapper<Args...>, callback));
  }

  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI |screen|.
  void ShowScreen(OobeScreen screen);
  // Show selected WebUI |screen|. Pass screen initialization using the |data|
  // parameter.
  void ShowScreenWithData(OobeScreen screen, const base::DictionaryValue* data);

  // Returns the OobeUI instance.
  OobeUI* GetOobeUI() const;

  // Returns current visible OOBE screen.
  OobeScreen GetCurrentScreen() const;

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

  // Returns the window which shows us.
  virtual gfx::NativeWindow GetNativeWindow();

  void SetBaseScreen(BaseScreen* base_screen);

 private:
  friend class OobeUI;

  // This function provides a unique name for every overload of
  // CallJavascriptFunctionUnsafe, allowing it to be used in base::Bind.
  template <typename... Args>
  void CallJavascriptFunctionImmediate(const std::string& function_name,
                                       const Args&... args) {
    web_ui()->CallJavascriptFunctionUnsafe(function_name, args...);
  }

  // Returns full name of JS method based on screen and method names.
  std::string FullMethodPath(const std::string& method) const;

  // Handles user action.
  void HandleUserAction(const std::string& action_id);

  // Handles situation when screen context is changed.
  void HandleContextChanged(const base::DictionaryValue* diff);

  // Keeps whether page is ready.
  bool page_is_ready_ = false;

  BaseScreen* base_screen_ = nullptr;

  // Full name of the corresponding JS screen object. Can be empty, if
  // there are no corresponding screen object or several different
  // objects.
  std::string js_screen_path_prefix_;

  // The string id used in the async asset load in JS. If it is set to a
  // non empty value, the Initialize will be deferred until the underlying load
  // is finished.
  std::string async_assets_load_id_;

  // Pending changes to context which will be sent when the page will be ready.
  base::DictionaryValue pending_context_changes_;

  JSCallsContainer* js_calls_container_ = nullptr;  // non-owning pointers.

  DISALLOW_COPY_AND_ASSIGN(BaseWebUIHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_
