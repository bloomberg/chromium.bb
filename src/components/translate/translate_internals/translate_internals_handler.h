// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_HANDLER_H_
#define COMPONENTS_TRANSLATE_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/variations/service/variations_service.h"

namespace base {
class ListValue;
class Value;
}  // namespace base

namespace translate {
struct LanguageDetectionDetails;
struct TranslateErrorDetails;
struct TranslateEventDetails;
struct TranslateInitDetails;

// The handler class for chrome://translate-internals page operations.
class TranslateInternalsHandler {
 public:
  TranslateInternalsHandler();

  TranslateInternalsHandler(const TranslateInternalsHandler&) = delete;
  TranslateInternalsHandler& operator=(const TranslateInternalsHandler&) =
      delete;

  ~TranslateInternalsHandler();

  // Returns a dictionary of languages where each key is a language
  // code and each value is a language name in the locale.
  static base::Value GetLanguages();

  virtual TranslateClient* GetTranslateClient() = 0;
  virtual variations::VariationsService* GetVariationsService() = 0;
  // Registers to handle |message| from JavaScript with |callback|.
  using MessageCallback =
      base::RepeatingCallback<void(base::Value::ConstListView)>;
  virtual void RegisterMessageCallback(const std::string& message,
                                       MessageCallback callback) = 0;

  // Always use RegisterMessageCallback() above in new code.
  //
  // TODO(crbug.com/1243386): Existing callers of
  // RegisterDeprecatedMessageCallback() should be migrated to
  // RegisterMessageCallback() if possible.
  //
  // Registers to handle |message| from JavaScript with |callback|.
  using DeprecatedMessageCallback =
      base::RepeatingCallback<void(const base::ListValue*)>;
  virtual void RegisterDeprecatedMessageCallback(
      const std::string& message,
      const DeprecatedMessageCallback& callback) = 0;
  // Calls a Javascript function with the given name and arguments.
  virtual void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) = 0;

 protected:
  // Subclasses should call this in order to handle messages from JavaScript.
  void RegisterMessageCallbacks();
  // Subclasses should call this when language detection details are available.
  void AddLanguageDetectionDetails(const LanguageDetectionDetails& details);

 private:
  // Callback for translate errors.
  void OnTranslateError(const TranslateErrorDetails& details);

  // Callback for translate initialisations.
  virtual void OnTranslateInit(const translate::TranslateInitDetails& details);

  // Callback for translate events.
  virtual void OnTranslateEvent(const TranslateEventDetails& details);

  // Handles the Javascript message 'removePrefItem'. This message is sent
  // when UI requests to remove an item in the preference.
  void OnRemovePrefItem(const base::ListValue* args);

  // Handles the JavaScript message 'setRecentTargetLanguage'. This message is
  // sent when the UI requests to change the 'translate_recent_target'
  // preference.
  void OnSetRecentTargetLanguage(const base::ListValue* args);

  // Handles the Javascript message 'overrideCountry'. This message is sent
  // when UI requests to override the stored country.
  void OnOverrideCountry(const base::ListValue* country);

  // Handles the Javascript message 'requestInfo'. This message is sent
  // when UI needs to show information concerned with the translation.
  // For now, this returns only prefs to Javascript.
  // |args| is not used.
  void OnRequestInfo(const base::ListValue* args);

  // Sends a message to Javascript.
  void SendMessageToJs(const std::string& message, const base::Value& value);

  // Sends the current preference to Javascript.
  void SendPrefsToJs();

  // Sends the languages currently supported by the server to JavaScript.
  void SendSupportedLanguagesToJs();

  // Sends the stored permanent country to Javascript.
  // |was_updated| tells Javascript if the country has been updated or not.
  void SendCountryToJs(bool was_updated);

  // Subscription for translate events coming from the translate language list.
  base::CallbackListSubscription event_subscription_;

  // Subscription for translate errors coming from the translate manager.
  base::CallbackListSubscription error_subscription_;

  // Subscription for translate initialization event.
  base::CallbackListSubscription init_subscription_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_HANDLER_H_
