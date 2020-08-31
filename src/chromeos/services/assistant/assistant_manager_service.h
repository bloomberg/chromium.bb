// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/component_export.h"
#include "chromeos/services/assistant/public/cpp/assistant_settings.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace chromeos {
namespace assistant {

// Interface class that defines all assistant functionalities.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantManagerService
    : public mojom::Assistant {
 public:
  class StateObserver;
  class CommunicationErrorObserver;

  struct UserInfo {
    UserInfo(const std::string& gaia_id, const std::string& access_token)
        : gaia_id(gaia_id), access_token(access_token) {}

    std::string gaia_id;
    std::string access_token;
  };

  enum State {
    // Initial state, the service is created but not started yet.
    STOPPED = 0,
    // Start has been called but libassistant creation is still in progress.
    // Calling |assistant_manager()| will still return a nullptr.
    STARTING = 1,
    // The service is started, libassistant has been created, but libassistant
    // is not ready yet to take requests.
    STARTED = 2,
    // The service is fully running and ready to take requests.
    RUNNING = 3
  };

  enum class CommunicationErrorType {
    AuthenticationError,
    Other,
  };

  ~AssistantManagerService() override = default;

  // Start the Assistant in the background with the given |user|.
  // If the user is nullopt, the service will be started in signed-out mode.
  // If you want to know when the service is started, use
  // |AddAndFireStateObserver| to add an observer.
  virtual void Start(const base::Optional<UserInfo>& user,
                     bool enable_hotword) = 0;

  // Stop the Assistant.
  virtual void Stop() = 0;

  // Return the current state.
  virtual State GetState() const = 0;

  // Set user information for Assistant. Passing a nullopt will reconfigure
  // Libassistant to run in signed-out mode, and passing a valid non-empty value
  // will switch the mode back to normal.
  virtual void SetUser(const base::Optional<UserInfo>& user) = 0;

  // Enable/disable ambient mode for Assistant.
  virtual void EnableAmbientMode(bool enabled) = 0;

  // Turn on / off all listening, including hotword and voice query.
  virtual void EnableListening(bool enable) = 0;

  // Turn on / off hotword listening.
  virtual void EnableHotword(bool enable) = 0;

  // Enable/disable ARC play store.
  virtual void SetArcPlayStoreEnabled(bool enabled) = 0;

  // Enable/disable Assistant Context.
  virtual void SetAssistantContextEnabled(bool enable) = 0;

  // Return a pointer of AssistantSettings.
  virtual AssistantSettings* GetAssistantSettings() = 0;

  // Add/Remove an observer that is invoked when there is a communication
  // error with the Assistant service.
  virtual void AddCommunicationErrorObserver(
      CommunicationErrorObserver* observer) = 0;
  virtual void RemoveCommunicationErrorObserver(
      const CommunicationErrorObserver* observer) = 0;

  // Add/Remove an observer that is invoked when there is a change in the
  // |AssistantManagerService::State| value.
  // When adding an observer it will immediately be triggered with the current
  // state value.
  virtual void AddAndFireStateObserver(StateObserver* observer) = 0;
  virtual void RemoveStateObserver(const StateObserver* observer) = 0;

  // Sync the device apps user consent status.
  virtual void SyncDeviceAppsStatus() = 0;

  // Update and sync the internal media player status to Libassistant.
  virtual void UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction action) = 0;
};

// Observes all state changes made to the |AssistantManagerService::State|.
class AssistantManagerService::StateObserver : public base::CheckedObserver {
 public:
  StateObserver() = default;
  ~StateObserver() override = default;

  virtual void OnStateChanged(AssistantManagerService::State new_state) = 0;
};

// Observes communication errors when communicating with the Assistant backend.
class AssistantManagerService::CommunicationErrorObserver
    : public base::CheckedObserver {
 public:
  CommunicationErrorObserver() = default;

  virtual void OnCommunicationError(CommunicationErrorType error) = 0;

 protected:
  ~CommunicationErrorObserver() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommunicationErrorObserver);
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
