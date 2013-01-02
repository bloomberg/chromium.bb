// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ENGINE_MODEL_SAFE_WORKER_H_
#define SYNC_INTERNAL_API_PUBLIC_ENGINE_MODEL_SAFE_WORKER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace base {
class DictionaryValue;
}  // namespace

namespace syncer {

// TODO(akalin): Move the non-exported functions in this file to a
// private header.

typedef base::Callback<enum SyncerError(void)> WorkCallback;

enum ModelSafeGroup {
  GROUP_PASSIVE = 0,   // Models that are just "passively" being synced; e.g.
                       // changes to these models don't need to be pushed to a
                       // native model.
  GROUP_UI,            // Models that live on UI thread and are being synced.
  GROUP_DB,            // Models that live on DB thread and are being synced.
  GROUP_FILE,          // Models that live on FILE thread and are being synced.
  GROUP_HISTORY,       // Models that live on history thread and are being
                       // synced.
  GROUP_PASSWORD,      // Models that live on the password thread and are
                       // being synced.  On windows and linux, this runs on the
                       // DB thread.
  MODEL_SAFE_GROUP_COUNT,
};

SYNC_EXPORT std::string ModelSafeGroupToString(ModelSafeGroup group);

// The Syncer uses a ModelSafeWorker for all tasks that could potentially
// modify syncable entries (e.g under a WriteTransaction). The ModelSafeWorker
// only knows how to do one thing, and that is take some work (in a fully
// pre-bound callback) and have it performed (as in Run()) from a thread which
// is guaranteed to be "model-safe", where "safe" refers to not allowing us to
// cause an embedding application model to fall out of sync with the
// syncable::Directory due to a race.
class SYNC_EXPORT ModelSafeWorker
    : public base::RefCountedThreadSafe<ModelSafeWorker> {
 public:
  // Any time the Syncer performs model modifications (e.g employing a
  // WriteTransaction), it should be done by this method to ensure it is done
  // from a model-safe thread.
  virtual SyncerError DoWorkAndWaitUntilDone(const WorkCallback& work) = 0;

  virtual ModelSafeGroup GetModelSafeGroup() = 0;

 protected:
  virtual ~ModelSafeWorker();

 private:
  friend class base::RefCountedThreadSafe<ModelSafeWorker>;
};

// A map that details which ModelSafeGroup each ModelType
// belongs to.  Routing info can change in response to the user enabling /
// disabling sync for certain types, as well as model association completions.
typedef std::map<ModelType, ModelSafeGroup> ModelSafeRoutingInfo;

// Caller takes ownership of return value.
SYNC_EXPORT_PRIVATE base::DictionaryValue* ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info);

SYNC_EXPORT std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info);

// Make a ModelTypeInvalidationMap for all the enabled types in a
// ModelSafeRoutingInfo using a default payload.
SYNC_EXPORT_PRIVATE ModelTypeInvalidationMap
    ModelSafeRoutingInfoToInvalidationMap(
        const ModelSafeRoutingInfo& routes,
        const std::string& payload);

SYNC_EXPORT ModelTypeSet GetRoutingInfoTypes(
    const ModelSafeRoutingInfo& routing_info);

SYNC_EXPORT ModelSafeGroup GetGroupForModelType(
    const ModelType type,
    const ModelSafeRoutingInfo& routes);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ENGINE_MODEL_SAFE_WORKER_H_
