// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enumerate the various item subtypes that are supported by sync.
// Each sync object is expected to have an immutable object type.
// An object's type is inferred from the type of data it holds.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNCABLE_MODEL_TYPE_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNCABLE_MODEL_TYPE_H_
#pragma once

#include <set>
#include <string>

#include "base/logging.h"
#include "base/time.h"
#include "sync/internal_api/public/util/enum_set.h"

namespace base {
class ListValue;
class StringValue;
class Value;
}

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}

namespace syncer {

namespace syncable {

enum ModelType {
  // Object type unknown.  Objects may transition through
  // the unknown state during their initial creation, before
  // their properties are set.  After deletion, object types
  // are generally preserved.
  UNSPECIFIED,
  // A permanent folder whose children may be of mixed
  // datatypes (e.g. the "Google Chrome" folder).
  TOP_LEVEL_FOLDER,

  // ------------------------------------ Start of "real" model types.
  // The model types declared before here are somewhat special, as they
  // they do not correspond to any browser data model.  The remaining types
  // are bona fide model types; all have a related browser data model and
  // can be represented in the protocol using a specific Message type in the
  // EntitySpecifics protocol buffer.
  //
  // A bookmark folder or a bookmark URL object.
  BOOKMARKS,
  FIRST_REAL_MODEL_TYPE = BOOKMARKS,  // Declared 2nd, for debugger prettiness.

  // A preference folder or a preference object.
  PREFERENCES,
  // A password folder or password object.
  PASSWORDS,
    // An AutofillProfile Object
  AUTOFILL_PROFILE,
  // An autofill folder or an autofill object.
  AUTOFILL,

  // A themes folder or a themes object.
  THEMES,
  // A typed_url folder or a typed_url object.
  TYPED_URLS,
  // An extension folder or an extension object.
  EXTENSIONS,
  // An object representing a set of Nigori keys.
  NIGORI,
  // An object representing a custom search engine.
  SEARCH_ENGINES,
  // An object representing a browser session.
  SESSIONS,
  // An app folder or an app object.
  APPS,
  // An app setting from the extension settings API.
  APP_SETTINGS,
  // An extension setting from the extension settings API.
  EXTENSION_SETTINGS,
  // App notifications.
  APP_NOTIFICATIONS,
  LAST_REAL_MODEL_TYPE = APP_NOTIFICATIONS,

  // If you are adding a new sync datatype that is exposed to the user via the
  // sync preferences UI, be sure to update the list in
  // chrome/browser/sync/user_selectable_sync_type.h so that the UMA histograms
  // for sync include your new type.

  MODEL_TYPE_COUNT,
};

typedef syncer::EnumSet<
  ModelType, FIRST_REAL_MODEL_TYPE, LAST_REAL_MODEL_TYPE> ModelTypeSet;
typedef syncer::EnumSet<
  ModelType, UNSPECIFIED, LAST_REAL_MODEL_TYPE> FullModelTypeSet;

inline ModelType ModelTypeFromInt(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, MODEL_TYPE_COUNT);
  return static_cast<ModelType>(i);
}

void AddDefaultFieldValue(syncable::ModelType datatype,
                          sync_pb::EntitySpecifics* specifics);

// Extract the model type of a SyncEntity protocol buffer.  ModelType is a
// local concept: the enum is not in the protocol.  The SyncEntity's ModelType
// is inferred from the presence of particular datatype field in the
// entity specifics.
ModelType GetModelType(const sync_pb::SyncEntity& sync_entity);

// Extract the model type from an EntitySpecifics field.  Note that there
// are some ModelTypes (like TOP_LEVEL_FOLDER) that can't be inferred this way;
// prefer using GetModelType where possible.
ModelType GetModelTypeFromSpecifics(const sync_pb::EntitySpecifics& specifics);

// If this returns false, we shouldn't bother maintaining a position
// value (sibling ordering) for this item.
bool ShouldMaintainPosition(ModelType model_type);

// Determine a model type from the field number of its associated
// EntitySpecifics field.
ModelType GetModelTypeFromSpecificsFieldNumber(int field_number);

// Return the field number of the EntitySpecifics field associated with
// a model type.
int GetSpecificsFieldNumberFromModelType(ModelType model_type);

// TODO(sync): The functions below badly need some cleanup.

// Returns a pointer to a string with application lifetime that represents
// the name of |model_type|.
const char* ModelTypeToString(ModelType model_type);

// Handles all model types, and not just real ones.
//
// Caller takes ownership of returned value.
base::StringValue* ModelTypeToValue(ModelType model_type);

// Converts a Value into a ModelType - complement to ModelTypeToValue().
ModelType ModelTypeFromValue(const base::Value& value);

// Returns the ModelType corresponding to the name |model_type_string|.
ModelType ModelTypeFromString(const std::string& model_type_string);

std::string ModelTypeSetToString(ModelTypeSet model_types);

// Caller takes ownership of returned list.
base::ListValue* ModelTypeSetToValue(ModelTypeSet model_types);

ModelTypeSet ModelTypeSetFromValue(const base::ListValue& value);

// Returns a string corresponding to the syncable tag for this datatype.
std::string ModelTypeToRootTag(ModelType type);

// Convert a real model type to a notification type (used for
// subscribing to server-issued notifications).  Returns true iff
// |model_type| was a real model type and |notification_type| was
// filled in.
bool RealModelTypeToNotificationType(ModelType model_type,
                                     std::string* notification_type);

// Converts a notification type to a real model type.  Returns true
// iff |notification_type| was the notification type of a real model
// type and |model_type| was filled in.
bool NotificationTypeToRealModelType(const std::string& notification_type,
                                     ModelType* model_type);

// Returns true if |model_type| is a real datatype
bool IsRealDataType(ModelType model_type);

}  // namespace syncable

}  // namespace syncer

// TODO(akalin): Move the names below to the 'syncer' namespace once
// we move this file to public/base.
namespace syncable {

using syncer::syncable::ModelType;
using syncer::syncable::ModelTypeSet;
using syncer::syncable::FullModelTypeSet;
using syncer::syncable::UNSPECIFIED;
using syncer::syncable::TOP_LEVEL_FOLDER;
using syncer::syncable::BOOKMARKS;
using syncer::syncable::FIRST_REAL_MODEL_TYPE;
using syncer::syncable::PREFERENCES;
using syncer::syncable::PASSWORDS;
using syncer::syncable::AUTOFILL_PROFILE;
using syncer::syncable::AUTOFILL;
using syncer::syncable::THEMES;
using syncer::syncable::TYPED_URLS;
using syncer::syncable::EXTENSIONS;
using syncer::syncable::NIGORI;
using syncer::syncable::SEARCH_ENGINES;
using syncer::syncable::SESSIONS;
using syncer::syncable::APPS;
using syncer::syncable::APP_SETTINGS;
using syncer::syncable::EXTENSION_SETTINGS;
using syncer::syncable::APP_NOTIFICATIONS;
using syncer::syncable::LAST_REAL_MODEL_TYPE;
using syncer::syncable::MODEL_TYPE_COUNT;
using syncer::syncable::ModelTypeFromInt;
using syncer::syncable::AddDefaultFieldValue;
using syncer::syncable::GetModelType;
using syncer::syncable::GetModelTypeFromSpecifics;
using syncer::syncable::ShouldMaintainPosition;
using syncer::syncable::GetModelTypeFromSpecificsFieldNumber;
using syncer::syncable::GetSpecificsFieldNumberFromModelType;
using syncer::syncable::ModelTypeToString;
using syncer::syncable::ModelTypeToValue;
using syncer::syncable::ModelTypeFromValue;
using syncer::syncable::ModelTypeFromString;
using syncer::syncable::ModelTypeSetToString;
using syncer::syncable::ModelTypeSetToValue;
using syncer::syncable::ModelTypeSetFromValue;
using syncer::syncable::ModelTypeToRootTag;
using syncer::syncable::RealModelTypeToNotificationType;
using syncer::syncable::NotificationTypeToRealModelType;
using syncer::syncable::IsRealDataType;

}  // namespace syncable

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNCABLE_MODEL_TYPE_H_
