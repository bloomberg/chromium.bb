// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_PRESENTER_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_PRESENTER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"

namespace ash {

class DeskTemplate;
class OverviewSession;

// DesksTemplatesPresenter is the presenter for the desks templates UI. It
// handles all calls to the model, and lets the UI know what to show or update.
// OverviewSession will create and own an instance of this object. It will be
// created after the desks bar is visible and destroyed once we receive an
// overview shutdown signal, to prevent calls to the model during shutdown.
class ASH_EXPORT DesksTemplatesPresenter : desks_storage::DeskModelObserver {
 public:
  explicit DesksTemplatesPresenter(OverviewSession* overview_session);
  DesksTemplatesPresenter(const DesksTemplatesPresenter&) = delete;
  DesksTemplatesPresenter& operator=(const DesksTemplatesPresenter&) = delete;
  ~DesksTemplatesPresenter() override;

  // Convenience function to get the presenter instance, which is created and
  // owned by `OverviewSession`.
  static DesksTemplatesPresenter* Get();

  bool should_show_templates_ui() { return should_show_templates_ui_; }

  size_t GetEntryCount() const;

  size_t GetMaxEntryCount() const;

  // Update the buttons of the desks templates UI and the visibility of the
  // templates grid. The grid contents are not updated. Updates
  // `should_show_templates_ui_`.
  void UpdateDesksTemplatesUI();

  // Calls the DeskModel to get all the template entries, with a callback to
  // `OnGetAllEntries`.
  void GetAllEntries();

  // Calls the DeskModel to delete the template with the provided uuid.
  void DeleteEntry(const std::string& template_uuid);

  // Launches the desk template with 'template_uuid' as a new desk.
  void LaunchDeskTemplate(const std::string& template_uuid);

  // Calls the DeskModel to capture the active desk as a template entry, with a
  // callback to `OnAddOrUpdateEntry`. If there are unsupported apps on the
  // active desk, a dialog will open up and we may or may not save the desk
  // asynchronously based on the user's decision.
  void MaybeSaveActiveDeskAsTemplate();

  // Saves or updates the `desk_template` to the model.
  void SaveOrUpdateDeskTemplate(bool is_update,
                                std::unique_ptr<DeskTemplate> desk_template);

  // desks_storage::DeskModelObserver:
  // TODO(sammiequon): Implement these once the model starts sending these
  // messages.
  void DeskModelLoaded() override;
  void OnDeskModelDestroying() override;
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<const DeskTemplate*>& new_entries) override {}
  void EntriesRemovedRemotely(const std::vector<std::string>& uuids) override {}
  void EntriesAddedOrUpdatedLocally(
      const std::vector<const DeskTemplate*>& new_entries) override {}
  void EntriesRemovedLocally(const std::vector<std::string>& uuids) override {}

 private:
  friend class DesksTemplatesPresenterTestApi;

  // Callback ran after querying the model for a list of entries. This function
  // also contains logic for updating the UI.
  void OnGetAllEntries(desks_storage::DeskModel::GetAllEntriesStatus status,
                       const std::vector<DeskTemplate*>& entries);

  // Callback after deleting an entry. Will then call `GetAllEntries` to update
  // the UI with the most up to date list of templates.
  void OnDeleteEntry(desks_storage::DeskModel::DeleteEntryStatus status);

  // Launches DeskTemplate after retrieval from storage.
  void OnGetTemplateForDeskLaunch(
      desks_storage::DeskModel::GetEntryByUuidStatus status,
      std::unique_ptr<DeskTemplate> entry);

  void OnAddOrUpdateEntry(
      bool was_update,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status);

  // Pointer to the session which owns `this`.
  OverviewSession* const overview_session_;

  base::ScopedObservation<desks_storage::DeskModel,
                          desks_storage::DeskModelObserver>
      desk_model_observation_{this};

  // If the user has at least one template entry, the desk templates ui should
  // be shown. Otherwise, it should be invisible.
  bool should_show_templates_ui_ = false;

  // Test closure that runs after the UI has been updated async after a call to
  // the model.
  base::OnceClosure on_update_ui_closure_for_testing_;

  base::WeakPtrFactory<DesksTemplatesPresenter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_PRESENTER_H_
