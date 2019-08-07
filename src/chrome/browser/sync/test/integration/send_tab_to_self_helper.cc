// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"

#include "base/logging.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

namespace send_tab_to_self_helper {

SendTabToSelfUrlChecker::SendTabToSelfUrlChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  service->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfUrlChecker::~SendTabToSelfUrlChecker() {
  service_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfUrlChecker::IsExitConditionSatisfied() {
  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  for (auto const& guid : model->GetAllGuids()) {
    if (model->GetEntryByGUID(guid)->GetURL() == url_) {
      return true;
    }
  }
  return false;
}

std::string SendTabToSelfUrlChecker::GetDebugMessage() const {
  return "Waiting for data for url '" + url_.spec() + "' to be populated.";
}

void SendTabToSelfUrlChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfUrlChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfUrlChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfModelEqualityChecker::SendTabToSelfModelEqualityChecker(
    send_tab_to_self::SendTabToSelfSyncService* service0,
    send_tab_to_self::SendTabToSelfSyncService* service1)
    : service0_(service0), service1_(service1) {
  service0->GetSendTabToSelfModel()->AddObserver(this);
  service1->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfModelEqualityChecker::~SendTabToSelfModelEqualityChecker() {
  service0_->GetSendTabToSelfModel()->RemoveObserver(this);
  service1_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfModelEqualityChecker::IsExitConditionSatisfied() {
  const send_tab_to_self::SendTabToSelfModel* model0 =
      service0_->GetSendTabToSelfModel();
  const send_tab_to_self::SendTabToSelfModel* model1 =
      service1_->GetSendTabToSelfModel();

  if (model0->GetAllGuids() != model1->GetAllGuids()) {
    return false;
  }
  for (auto const& guid : model0->GetAllGuids()) {
    const send_tab_to_self::SendTabToSelfEntry* entry0 =
        model0->GetEntryByGUID(guid);
    const send_tab_to_self::SendTabToSelfEntry* entry1 =
        model1->GetEntryByGUID(guid);

    DCHECK_NE(entry0, nullptr);
    DCHECK_NE(entry1, nullptr);

    if (entry0->GetGUID() != entry1->GetGUID() ||
        entry0->GetURL() != entry1->GetURL() ||
        entry0->GetTitle() != entry1->GetTitle() ||
        entry0->GetSharedTime() != entry1->GetSharedTime() ||
        entry0->GetOriginalNavigationTime() !=
            entry1->GetOriginalNavigationTime() ||
        entry0->GetDeviceName() != entry1->GetDeviceName()) {
      return false;
    }
  }
  return true;
}

std::string SendTabToSelfModelEqualityChecker::GetDebugMessage() const {
  return "Waiting for services to converge";
}

void SendTabToSelfModelEqualityChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfModelEqualityChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfModelEqualityChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfActiveChecker::SendTabToSelfActiveChecker(
    send_tab_to_self::SendTabToSelfSyncService* service)
    : service_(service) {
  service->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfActiveChecker::~SendTabToSelfActiveChecker() {
  service_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfActiveChecker::IsExitConditionSatisfied() {
  return service_->GetSendTabToSelfModel()->IsReady();
}

std::string SendTabToSelfActiveChecker::GetDebugMessage() const {
  return "Waiting for model to be active.";
}

void SendTabToSelfActiveChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfActiveChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfActiveChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfMultiDeviceActiveChecker::SendTabToSelfMultiDeviceActiveChecker(
    syncer::DeviceInfoTracker* tracker)
    : tracker_(tracker) {
  tracker_->AddObserver(this);
}

SendTabToSelfMultiDeviceActiveChecker::
    ~SendTabToSelfMultiDeviceActiveChecker() {
  tracker_->RemoveObserver(this);
}

bool SendTabToSelfMultiDeviceActiveChecker::IsExitConditionSatisfied() {
  return tracker_->CountActiveDevices() > 1;
}

std::string SendTabToSelfMultiDeviceActiveChecker::GetDebugMessage() const {
  return "Waiting for multiple devices to be active.";
}

void SendTabToSelfMultiDeviceActiveChecker::OnDeviceInfoChange() {
  CheckExitCondition();
}

}  // namespace send_tab_to_self_helper
