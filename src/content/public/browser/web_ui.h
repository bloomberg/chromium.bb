// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

class WebContents;
class WebUIController;
class WebUIMessageHandler;

// A WebUI sets up the datasources and message handlers for a given HTML-based
// UI.
class CONTENT_EXPORT WebUI {
 public:
  // An opaque identifier used to identify a WebUI. This can only be compared to
  // kNoWebUI or other WebUI types. See GetWebUIType.
  typedef const void* TypeID;

  // A special WebUI type that signifies that a given page would not use the
  // Web UI system.
  static const TypeID kNoWebUI;

  // Returns JavaScript code that, when executed, calls the function specified
  // by |function_name| with the arguments specified in |arg_list|.
  static base::string16 GetJavascriptCall(
      const std::string& function_name,
      const std::vector<const base::Value*>& arg_list);

  virtual ~WebUI() {}

  virtual WebContents* GetWebContents() = 0;

  virtual WebUIController* GetController() = 0;
  virtual void SetController(std::unique_ptr<WebUIController> controller) = 0;

  // Returns the device scale factor of the monitor that the renderer is on.
  // Whenever possible, WebUI should push resources with this scale factor to
  // Javascript.
  virtual float GetDeviceScaleFactor() = 0;

  // Gets a custom tab title provided by the Web UI. If there is no title
  // override, the string will be empty which should trigger the default title
  // behavior for the tab.
  virtual const base::string16& GetOverriddenTitle() = 0;
  virtual void OverrideTitle(const base::string16& title) = 0;

  // Allows a controller to override the BindingsPolicy that should be enabled
  // for this page.
  virtual int GetBindings() = 0;
  virtual void SetBindings(int bindings) = 0;

  // Allows a scheme to be requested which is provided by the WebUIController.
  virtual const std::vector<std::string>& GetRequestableSchemes() = 0;
  virtual void AddRequestableScheme(const char* scheme) = 0;

  virtual void AddMessageHandler(
      std::unique_ptr<WebUIMessageHandler> handler) = 0;

  // Used by WebUIMessageHandlers. If the given message is already registered,
  // the call has no effect.
  using MessageCallback = base::RepeatingCallback<void(const base::ListValue*)>;
  virtual void RegisterMessageCallback(base::StringPiece message,
                                       const MessageCallback& callback) = 0;

  template <typename... Args>
  void RegisterHandlerCallback(
      base::StringPiece message,
      base::RepeatingCallback<void(Args...)> callback) {
    RegisterMessageCallback(
        message, base::BindRepeating(
                     &Call<std::index_sequence_for<Args...>, Args...>::Impl,
                     callback, message));
  }

  // This is only needed if an embedder overrides handling of a WebUIMessage and
  // then later wants to undo that, or to route it to a different WebUI object.
  virtual void ProcessWebUIMessage(const GURL& source_url,
                                   const std::string& message,
                                   const base::ListValue& args) = 0;

  // Returns true if this WebUI can currently call JavaScript.
  virtual bool CanCallJavascript() = 0;

  // Calling these functions directly is discouraged. It's generally preferred
  // to call WebUIMessageHandler::CallJavascriptFunction, as that has
  // lifecycle controls to prevent calling JavaScript before the page is ready.
  //
  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  //
  // All function names in WebUI must consist of only ASCII characters.
  // There are variants for calls with more arguments.
  virtual void CallJavascriptFunctionUnsafe(
      const std::string& function_name) = 0;
  virtual void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                            const base::Value& arg) = 0;
  virtual void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                            const base::Value& arg1,
                                            const base::Value& arg2) = 0;
  virtual void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                            const base::Value& arg1,
                                            const base::Value& arg2,
                                            const base::Value& arg3) = 0;
  virtual void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                            const base::Value& arg1,
                                            const base::Value& arg2,
                                            const base::Value& arg3,
                                            const base::Value& arg4) = 0;
  virtual void CallJavascriptFunctionUnsafe(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) = 0;

  // Allows mutable access to this WebUI's message handlers for testing.
  virtual std::vector<std::unique_ptr<WebUIMessageHandler>>*
  GetHandlersForTesting() = 0;

 private:
  template <typename T>
  static T GetValue(const base::Value& value);

  template <>
  inline bool GetValue<bool>(const base::Value& value) {
    return value.GetBool();
  }

  template <>
  inline int GetValue<int>(const base::Value& value) {
    return value.GetInt();
  }

  template <>
  inline const std::string& GetValue<const std::string&>(
      const base::Value& value) {
    return value.GetString();
  }

  template <typename Is, typename... Args>
  struct Call;

  template <size_t... Is, typename... Args>
  struct Call<std::index_sequence<Is...>, Args...> {
    static void Impl(base::RepeatingCallback<void(Args...)> callback,
                     base::StringPiece message,
                     const base::ListValue* list) {
      base::span<const base::Value> args = list->GetList();
      CHECK_EQ(args.size(), sizeof...(Args)) << message;
      callback.Run(GetValue<Args>(args[Is])...);
    }
  };
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_H_
