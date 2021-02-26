// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/webui/sync_internals/sync_internals_message_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_internals_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/model/type_entities_count.h"
#include "ios/components/webui/web_ui_provider.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/webui/web_ui_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the initial state of the "include specifics" flag, based on whether
// or not the corresponding command-line switch is set.
bool GetIncludeSpecificsInitialState() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSyncIncludeSpecificsInProtocolLog);
}

}  // namespace

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : include_specifics_(GetIncludeSpecificsInitialState()),
      weak_ptr_factory_(this) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  if (js_controller_) {
    js_controller_->RemoveJsEventHandler(this);
  }

  syncer::SyncService* service = GetSyncService();
  if (service && service->HasObserver(this)) {
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
  }
}

void SyncInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestDataAndRegisterForUpdates,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestListOfTypes,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestListOfTypes,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestIncludeSpecificsInitialState,
      base::BindRepeating(&SyncInternalsMessageHandler::
                              HandleRequestIncludeSpecificsInitialState,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kSetIncludeSpecifics,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleSetIncludeSpecifics,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStart,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleRequestStart,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStopKeepData,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestStopKeepData,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStopClearData,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestStopClearData,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kTriggerRefresh,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleTriggerRefresh,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kGetAllNodes,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleGetAllNodes,
                          base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates(
    const base::ListValue* args) {
  DCHECK(args->empty());

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  syncer::SyncService* service = GetSyncService();
  if (service && !is_registered_) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);
    js_controller_ = service->GetJsController();
    js_controller_->AddJsEventHandler(this);
    is_registered_ = true;
  }

  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const base::ListValue* args) {
  DCHECK(args->empty());
  base::DictionaryValue event_details;
  auto type_list = std::make_unique<base::ListValue>();
  syncer::ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (syncer::ModelType type : protocol_types) {
    type_list->AppendString(ModelTypeToString(type));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, std::move(type_list));
  DispatchEvent(syncer::sync_ui_util::kOnReceivedListOfTypes, event_details);
}

void SyncInternalsMessageHandler::HandleRequestIncludeSpecificsInitialState(
    const base::ListValue* args) {
  DCHECK(args->empty());

  base::DictionaryValue value;
  value.SetBoolean(syncer::sync_ui_util::kIncludeSpecifics,
                   GetIncludeSpecificsInitialState());

  DispatchEvent(syncer::sync_ui_util::kOnReceivedIncludeSpecificsInitialState,
                value);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  int request_id = 0;
  bool success = ExtractIntegerValue(args, &request_id);
  DCHECK(success);

  syncer::SyncService* service = GetSyncService();
  if (service) {
    service->GetAllNodesForDebugging(
        base::BindOnce(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                       weak_ptr_factory_.GetWeakPtr(), request_id));
  }
}

void SyncInternalsMessageHandler::HandleSetIncludeSpecifics(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  include_specifics_ = args->GetList()[0].GetBool();
}

void SyncInternalsMessageHandler::HandleRequestStart(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->GetUserSettings()->SetSyncRequested(true);
  // If the service was previously stopped with CLEAR_DATA, then the
  // "first-setup-complete" bit was also cleared, and now the service wouldn't
  // fully start up. So set that too.
  service->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
}

void SyncInternalsMessageHandler::HandleRequestStopKeepData(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->GetUserSettings()->SetSyncRequested(false);
}

void SyncInternalsMessageHandler::HandleRequestStopClearData(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->StopAndClear();
}

void SyncInternalsMessageHandler::HandleTriggerRefresh(
    const base::ListValue* args) {
  syncer::SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  // Only allowed to trigger refresh/schedule nudges for protocol types, things
  // like PROXY_TABS are not allowed.
  service->TriggerRefresh(syncer::Intersection(service->GetActiveDataTypes(),
                                               syncer::ProtocolTypes()));
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    int request_id,
    std::unique_ptr<base::ListValue> nodes) {
  base::Value id(request_id);
  base::Value nodes_clone = nodes->Clone();

  std::vector<const base::Value*> args{&id, &nodes_clone};
  web_ui()->CallJavascriptFunction(syncer::sync_ui_util::kGetAllNodesCallback,
                                   args);
}

void SyncInternalsMessageHandler::OnStateChanged(syncer::SyncService* sync) {
  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  std::unique_ptr<base::DictionaryValue> value(
      event.ToValue(include_specifics_));
  DispatchEvent(syncer::sync_ui_util::kOnProtocolEvent, *value);
}

void SyncInternalsMessageHandler::HandleJsEvent(
    const std::string& name,
    const syncer::JsEventDetails& details) {
  DVLOG(1) << "Handling event: " << name << " with details "
           << details.ToString();
  DispatchEvent(name, details.Get());
}

void SyncInternalsMessageHandler::SendAboutInfoAndEntityCounts() {
  // This class serves to display debug information to the user, so it's fine to
  // include sensitive data in ConstructAboutInformation().
  std::unique_ptr<base::DictionaryValue> value =
      syncer::sync_ui_util::ConstructAboutInformation(
          syncer::sync_ui_util::IncludeSensitiveData(true), GetSyncService(),
          web_ui::GetChannel());
  DispatchEvent(syncer::sync_ui_util::kOnAboutInfoUpdated, *value);

  if (syncer::SyncService* service = GetSyncService()) {
    service->GetEntityCountsForDebugging(
        BindOnce(&SyncInternalsMessageHandler::OnGotEntityCounts,
                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnGotEntityCounts({});
  }
}

void SyncInternalsMessageHandler::OnGotEntityCounts(
    const std::vector<syncer::TypeEntitiesCount>& entity_counts) {
  base::ListValue count_list;
  for (const syncer::TypeEntitiesCount& count : entity_counts) {
    base::DictionaryValue count_dictionary;
    count_dictionary.SetStringPath(syncer::sync_ui_util::kModelType,
                                   ModelTypeToString(count.type));
    count_dictionary.SetIntPath(syncer::sync_ui_util::kEntities,
                                count.entities);
    count_dictionary.SetIntPath(syncer::sync_ui_util::kNonTombstoneEntities,
                                count.non_tombstone_entities);
    count_list.Append(std::move(count_dictionary));
  }

  base::DictionaryValue event_details;
  event_details.SetPath(syncer::sync_ui_util::kEntityCounts,
                        std::move(count_list));
  DispatchEvent(syncer::sync_ui_util::kOnEntityCountsUpdated,
                std::move(event_details));
}

// Gets the SyncService of the underlying original profile. May return null.
syncer::SyncService* SyncInternalsMessageHandler::GetSyncService() {
  return web_ui::GetSyncServiceForWebUI(web_ui());
}

void SyncInternalsMessageHandler::DispatchEvent(
    const std::string& name,
    const base::Value& details_value) {
  base::Value event_name = base::Value(name);

  std::vector<const base::Value*> args{&event_name, &details_value};

  web_ui()->CallJavascriptFunction(syncer::sync_ui_util::kDispatchEvent, args);
}
