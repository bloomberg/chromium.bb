// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

// Returns whether the item should be ignored by the holding space model. This
// returns true if the item is not supported in the current context, but may
// be otherwise supported. For example, returns true for ARC file system
// backed items in a secondary user profile.
bool ShouldIgnoreItem(Profile* profile, const HoldingSpaceItem* item) {
  return file_manager::util::GetAndroidFilesPath().IsParent(
             item->file_path()) &&
         !chromeos::ProfileHelper::IsPrimaryProfile(profile);
}

}  // namespace

// static
constexpr char HoldingSpacePersistenceDelegate::kPersistencePath[];

HoldingSpacePersistenceDelegate::HoldingSpacePersistenceDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model,
    ThumbnailLoader* thumbnail_loader,
    PersistenceRestoredCallback persistence_restored_callback)
    : HoldingSpaceKeyedServiceDelegate(service, model),
      thumbnail_loader_(thumbnail_loader),
      persistence_restored_callback_(std::move(persistence_restored_callback)) {
}

HoldingSpacePersistenceDelegate::~HoldingSpacePersistenceDelegate() = default;

// static
void HoldingSpacePersistenceDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPersistencePath);
}

void HoldingSpacePersistenceDelegate::Init() {
  // We expect that the associated profile is already ready when we are being
  // initialized. That being the case, we can immediately proceed to restore
  // the holding space model from persistence storage.
  RestoreModelFromPersistence();
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (is_restoring_persistence())
    return;

  // Write the new finalized `items` to persistent storage.
  ListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  for (const HoldingSpaceItem* item : items) {
    if (item->progress().IsComplete())
      update->Append(item->Serialize());
  }
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (is_restoring_persistence())
    return;

  // Remove the `items` from persistent storage.
  ListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  update->EraseListValueIf([&items](const base::Value& persisted_item) {
    const std::string& persisted_item_id = HoldingSpaceItem::DeserializeId(
        base::Value::AsDictionaryValue(persisted_item));
    return std::any_of(items.begin(), items.end(),
                       [&persisted_item_id](const HoldingSpaceItem* item) {
                         return persisted_item_id == item->id();
                       });
  });
}

void HoldingSpacePersistenceDelegate::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item,
    uint32_t updated_fields) {
  if (is_restoring_persistence())
    return;

  // Only finalized items are persisted.
  if (!item->progress().IsComplete())
    return;

  // Attempt to find the finalized `item` in persistent storage.
  ListPrefUpdate update(profile()->GetPrefs(), kPersistencePath);
  auto item_it = std::find_if(
      update->GetList().begin(), update->GetList().end(),
      [&item](const base::Value& persisted_item) {
        return HoldingSpaceItem::DeserializeId(base::Value::AsDictionaryValue(
                   persisted_item)) == item->id();
      });

  // If the finalized `item` already exists in persistent storage, update it.
  if (item_it != update->GetList().end()) {
    *item_it = item->Serialize();
    return;
  }

  // If the finalized `item` did not previously exist in persistent storage,
  // insert it at the appropriate index.
  item_it = update->GetList().begin();
  for (const auto& candidate_item : model()->items()) {
    if (candidate_item.get() == item) {
      update->Insert(item_it, item->Serialize());
      return;
    }
    if (candidate_item->progress().IsComplete())
      ++item_it;
  }

  // The finalized `item` should exist in the model and be handled above.
  NOTREACHED();
}

void HoldingSpacePersistenceDelegate::RestoreModelFromPersistence() {
  DCHECK(model()->items().empty());

  const auto* persisted_holding_space_items =
      profile()->GetPrefs()->GetList(kPersistencePath);

  // If persistent storage is empty we can immediately notify the callback of
  // persistence restoration completion and quit early.
  if (persisted_holding_space_items->GetList().empty()) {
    std::move(persistence_restored_callback_).Run();
    return;
  }

  for (const auto& persisted_holding_space_item :
       persisted_holding_space_items->GetList()) {
    std::unique_ptr<HoldingSpaceItem> holding_space_item =
        HoldingSpaceItem::Deserialize(
            base::Value::AsDictionaryValue(persisted_holding_space_item),
            base::BindOnce(&holding_space_util::ResolveImage,
                           base::Unretained(thumbnail_loader_)));

    if (!ShouldIgnoreItem(profile(), holding_space_item.get()))
      service()->AddItem(std::move(holding_space_item));
  }

  // Notify completion of persistence restoration.
  std::move(persistence_restored_callback_).Run();
}

}  // namespace ash
