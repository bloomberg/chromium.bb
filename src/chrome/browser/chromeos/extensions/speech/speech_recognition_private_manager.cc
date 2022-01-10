// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/extensions/api/speech_recognition_private.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace {

std::string GetExtensionIdFromKey(const std::string& key) {
  std::size_t pos = key.find('.');
  if (pos != std::string::npos)
    return key.substr(0, pos);

  // If we couldn't find a ".", then the key is the extension ID.
  return key;
}

absl::optional<int> GetClientIdFromKey(const std::string& key) {
  int client_id = -1;
  absl::optional<int> result;
  std::size_t pos = key.find('.');
  if (pos != std::string::npos) {
    // Extract the number to the right of the "."
    base::StringToInt(key.substr(pos + 1), &client_id);
    result = client_id;
  }

  return result;
}

// Factory to get or create an instance of SpeechRecognitionPrivateManager from
// a browser context.
class SpeechRecognitionPrivateManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  SpeechRecognitionPrivateManagerFactory(
      const SpeechRecognitionPrivateManagerFactory&) = delete;
  SpeechRecognitionPrivateManagerFactory& operator=(
      const SpeechRecognitionPrivateManagerFactory&) = delete;

  static SpeechRecognitionPrivateManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<SpeechRecognitionPrivateManagerFactory>;
  static SpeechRecognitionPrivateManagerFactory* GetInstance();

  SpeechRecognitionPrivateManagerFactory();
  ~SpeechRecognitionPrivateManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

// static
SpeechRecognitionPrivateManager*
SpeechRecognitionPrivateManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SpeechRecognitionPrivateManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SpeechRecognitionPrivateManagerFactory*
SpeechRecognitionPrivateManagerFactory::GetInstance() {
  static base::NoDestructor<SpeechRecognitionPrivateManagerFactory> instance;
  return instance.get();
}

SpeechRecognitionPrivateManagerFactory::SpeechRecognitionPrivateManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "SpeechRecognitionApiManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(EventRouterFactory::GetInstance());
}

KeyedService* SpeechRecognitionPrivateManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SpeechRecognitionPrivateManager(context);
}

content::BrowserContext*
SpeechRecognitionPrivateManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Incognito profiles should use their own instance of the browser context.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace

SpeechRecognitionPrivateManager::SpeechRecognitionPrivateManager(
    content::BrowserContext* context)
    : context_(context) {}

SpeechRecognitionPrivateManager::~SpeechRecognitionPrivateManager() = default;

// static
SpeechRecognitionPrivateManager* SpeechRecognitionPrivateManager::Get(
    content::BrowserContext* context) {
  return static_cast<SpeechRecognitionPrivateManagerFactory*>(GetFactory())
      ->GetForBrowserContext(context);
}

// static
BrowserContextKeyedServiceFactory*
SpeechRecognitionPrivateManager::GetFactory() {
  static base::NoDestructor<SpeechRecognitionPrivateManagerFactory> g_factory;
  return g_factory.get();
}

void SpeechRecognitionPrivateManager::HandleStart(
    const std::string& key,
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    OnStartCallback callback) {
  GetSpeechRecognizer(key)->HandleStart(locale, interim_results,
                                        std::move(callback));
}

void SpeechRecognitionPrivateManager::HandleStop(const std::string& key,
                                                 OnStopCallback callback) {
  GetSpeechRecognizer(key)->HandleStop(std::move(callback));
}

void SpeechRecognitionPrivateManager::HandleSpeechRecognitionStopped(
    const std::string& key) {
  std::string extension_id = GetExtensionIdFromKey(key);
  absl::optional<int> client_id = GetClientIdFromKey(key);
  EventRouter* event_router = EventRouter::Get(context_);

  base::Value return_dict(base::Value::Type::DICTIONARY);
  if (client_id.has_value())
    return_dict.SetIntKey("clientId", client_id.value());

  auto event_args = std::vector<base::Value>();
  event_args.push_back(std::move(return_dict));
  std::unique_ptr<Event> event = std::make_unique<Event>(
      events::SPEECH_RECOGNITION_PRIVATE_ON_STOP,
      api::speech_recognition_private::OnStop::kEventName,
      std::move(event_args));

  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void SpeechRecognitionPrivateManager::HandleSpeechRecognitionResult(
    const std::string& key,
    const std::u16string& transcript,
    bool is_final) {
  std::string extension_id = GetExtensionIdFromKey(key);
  absl::optional<int> client_id = GetClientIdFromKey(key);
  EventRouter* event_router = EventRouter::Get(context_);

  api::speech_recognition_private::SpeechRecognitionResultEvent event;
  event.transcript = base::UTF16ToUTF8(transcript);
  event.is_final = is_final;
  if (client_id)
    event.client_id = std::make_unique<int>(*client_id);

  auto event_args = api::speech_recognition_private::OnResult::Create(event);
  std::unique_ptr<Event> event_ptr = std::make_unique<Event>(
      events::SPEECH_RECOGNITION_PRIVATE_ON_RESULT,
      api::speech_recognition_private::OnResult::kEventName,
      std::move(event_args));

  event_router->DispatchEventToExtension(extension_id, std::move(event_ptr));
}

void SpeechRecognitionPrivateManager::HandleSpeechRecognitionError(
    const std::string& key,
    const std::string& message) {
  std::string extension_id = GetExtensionIdFromKey(key);
  absl::optional<int> client_id = GetClientIdFromKey(key);
  EventRouter* event_router = EventRouter::Get(context_);

  api::speech_recognition_private::SpeechRecognitionErrorEvent event;
  event.message = message;
  if (client_id)
    event.client_id = std::make_unique<int>(*client_id);

  auto event_args = api::speech_recognition_private::OnError::Create(event);
  std::unique_ptr<Event> event_ptr = std::make_unique<Event>(
      events::SPEECH_RECOGNITION_PRIVATE_ON_ERROR,
      api::speech_recognition_private::OnError::kEventName,
      std::move(event_args));

  event_router->DispatchEventToExtension(extension_id, std::move(event_ptr));
}

std::string SpeechRecognitionPrivateManager::CreateKey(
    const std::string& extension_id,
    absl::optional<int> client_id) {
  if (!client_id.has_value())
    return extension_id;

  return base::StringPrintf("%s.%d", extension_id.c_str(), *client_id);
}

SpeechRecognitionPrivateRecognizer*
SpeechRecognitionPrivateManager::GetSpeechRecognizer(const std::string& key) {
  auto& recognizer = recognition_data_[key];
  if (!recognizer)
    recognizer = std::make_unique<SpeechRecognitionPrivateRecognizer>(
        this, context_, key);

  return recognizer.get();
}

}  // namespace extensions
