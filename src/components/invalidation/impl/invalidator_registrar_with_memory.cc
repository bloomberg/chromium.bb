// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

constexpr char kTopicsToHandlerDeprecated[] = "invalidation.topics_to_handler";

constexpr char kTopicsToHandler[] = "invalidation.per_sender_topics_to_handler";

constexpr char kHandler[] = "handler";
constexpr char kIsPublic[] = "is_public";

// Added in M76.
void MigratePrefs(PrefService* prefs, const std::string& sender_id) {
  auto* old_prefs = prefs->GetDictionary(kTopicsToHandlerDeprecated);
  if (old_prefs->empty()) {
    return;
  }
  {
    DictionaryPrefUpdate update(prefs, kTopicsToHandler);
    update->SetKey(sender_id, old_prefs->Clone());
  }
  prefs->ClearPref(kTopicsToHandlerDeprecated);
}

}  // namespace

// static
void InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTopicsToHandlerDeprecated);
  registry->RegisterDictionaryPref(kTopicsToHandler);
}

InvalidatorRegistrarWithMemory::InvalidatorRegistrarWithMemory(
    PrefService* local_state,
    const std::string& sender_id,
    bool migrate_old_prefs)
    : InvalidatorRegistrar(), local_state_(local_state), sender_id_(sender_id) {
  if (migrate_old_prefs) {
    MigratePrefs(local_state_, sender_id_);
  }
  const base::Value* pref_data =
      local_state_->Get(kTopicsToHandler)->FindDictKey(sender_id_);
  if (!pref_data) {
    DictionaryPrefUpdate update(local_state_, kTopicsToHandler);
    update->SetKey(sender_id_, base::DictionaryValue());
    return;
  }
  for (const auto& it : pref_data->DictItems()) {
    Topic topic = it.first;
    if (it.second.is_dict()) {
      const auto* handler = it.second.FindDictKey(kHandler);
      const auto* is_public = it.second.FindDictKey(kIsPublic);
      if (!handler || !is_public) {
        continue;
      }
      handler_name_to_topics_map_[handler->GetString()].emplace(
          topic, TopicMetadata{is_public->GetBool()});
    } else if (it.second.is_string()) {
      std::string handler_name;
      it.second.GetAsString(&handler_name);
      handler_name_to_topics_map_[handler_name].emplace(topic,
                                                        TopicMetadata{false});
    }
  }
}

InvalidatorRegistrarWithMemory::~InvalidatorRegistrarWithMemory() {}

bool InvalidatorRegistrarWithMemory::UpdateRegisteredTopics(
    InvalidationHandler* handler,
    const Topics& topics) {
  Topics old_topics = GetRegisteredTopics(handler);
  bool success = InvalidatorRegistrar::UpdateRegisteredTopics(handler, topics);
  if (!InvalidatorRegistrar::IsHandlerRegistered(handler)) {
    return success;
  }

  DictionaryPrefUpdate update(local_state_, kTopicsToHandler);
  base::Value* pref_data = update->FindDictKey(sender_id_);
  auto to_unregister = FindRemovedTopics(old_topics, topics);
  if (!to_unregister.empty()) {
    for (const auto& topic : to_unregister) {
      pref_data->RemoveKey(topic);
      handler_name_to_topics_map_[handler->GetOwnerName()].erase(topic);
    }
  }

  for (const auto& topic : topics) {
    handler_name_to_topics_map_[handler->GetOwnerName()].insert(topic);
    base::DictionaryValue handler_pref;
    handler_pref.SetStringKey(kHandler, handler->GetOwnerName());
    handler_pref.SetBoolKey(kIsPublic, topic.second.is_public);
    pref_data->SetKey(topic.first, std::move(handler_pref));
  }
  return success;
}

Topics InvalidatorRegistrarWithMemory::GetAllRegisteredIds() const {
  Topics registered_topics;
  for (const auto& handler_to_topic : handler_name_to_topics_map_) {
    registered_topics.insert(handler_to_topic.second.begin(),
                             handler_to_topic.second.end());
  }
  return registered_topics;
}

void InvalidatorRegistrarWithMemory::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> callback)
    const {
  callback.Run(CollectDebugData());
}

base::DictionaryValue InvalidatorRegistrarWithMemory::CollectDebugData() const {
  base::DictionaryValue return_value;
  return_value.SetInteger("InvalidatorRegistrarWithMemory.Handlers",
                          handler_name_to_topics_map_.size());
  for (const auto& handler_to_topics : handler_name_to_topics_map_) {
    const std::string& handler = handler_to_topics.first;
    for (const auto& topic : handler_to_topics.second) {
      return_value.SetString("InvalidatorRegistrarWithMemory." + topic.first,
                             handler);
    }
  }
  return return_value;
}

}  // namespace syncer
