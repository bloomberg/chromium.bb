// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/var_tracker.h"

#include <limits>

#include "base/logging.h"
#include "ppapi/shared_impl/id_assignment.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

VarTracker::VarInfo::VarInfo()
    : var(),
      ref_count(0),
      track_with_no_reference_count(0) {
}

VarTracker::VarInfo::VarInfo(Var* v, int input_ref_count)
    : var(v),
      ref_count(input_ref_count),
      track_with_no_reference_count(0) {
}

VarTracker::VarTracker() : last_var_id_(0) {
}

VarTracker::~VarTracker() {
}

int32 VarTracker::AddVar(Var* var) {
  return AddVarInternal(var, ADD_VAR_TAKE_ONE_REFERENCE);
}

Var* VarTracker::GetVar(int32 var_id) const {
  VarMap::const_iterator result = live_vars_.find(var_id);
  if (result == live_vars_.end())
    return NULL;
  return result->second.var.get();
}

Var* VarTracker::GetVar(const PP_Var& var) const {
  if (!IsVarTypeRefcounted(var.type))
    return NULL;
  return GetVar(static_cast<int32>(var.value.as_id));
}

bool VarTracker::AddRefVar(int32 var_id) {
  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::iterator found = live_vars_.find(var_id);
  if (found == live_vars_.end()) {
    NOTREACHED();  // Invalid var.
    return false;
  }

  VarInfo& info = found->second;
  if (info.ref_count == 0) {
    // All live vars with no refcount should be tracked objects.
    DCHECK(info.track_with_no_reference_count > 0);
    DCHECK(info.var->GetType() == PP_VARTYPE_OBJECT);

    TrackedObjectGettingOneRef(found);
  }

  // Basic refcount increment.
  info.ref_count++;
  return true;
}

bool VarTracker::AddRefVar(const PP_Var& var) {
  if (!IsVarTypeRefcounted(var.type))
    return false;
  return AddRefVar(static_cast<int32>(var.value.as_id));
}

bool VarTracker::ReleaseVar(int32 var_id) {
  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::iterator found = live_vars_.find(var_id);
  if (found == live_vars_.end()) {
    NOTREACHED() << "Unref-ing an invalid var";
    return false;
  }

  VarInfo& info = found->second;
  if (info.ref_count == 0) {
    NOTREACHED() << "Releasing an object with zero ref";
    return false;
  }
  info.ref_count--;

  if (info.ref_count == 0) {
    if (info.var->GetType() == PP_VARTYPE_OBJECT) {
      // Objects have special requirements and may not necessarily be released
      // when the refcount goes to 0.
      ObjectGettingZeroRef(found);
    } else {
      // All other var types can just be released.
      DCHECK(info.track_with_no_reference_count == 0);
      live_vars_.erase(found);
    }
  }
  return true;
}

bool VarTracker::ReleaseVar(const PP_Var& var) {
  if (!IsVarTypeRefcounted(var.type))
    return false;
  return ReleaseVar(static_cast<int32>(var.value.as_id));
}

int32 VarTracker::AddVarInternal(Var* var, AddVarRefMode mode) {
  // If the plugin manages to create millions of strings.
  if (last_var_id_ == std::numeric_limits<int32>::max() >> kPPIdTypeBits)
    return 0;

  int32 new_id = MakeTypedId(++last_var_id_, PP_ID_TYPE_VAR);
  live_vars_.insert(std::make_pair(new_id,
      VarInfo(var, mode == ADD_VAR_TAKE_ONE_REFERENCE ? 1 : 0)));

  return new_id;
}

VarTracker::VarMap::iterator VarTracker::GetLiveVar(int32 id) {
  return live_vars_.find(id);
}

VarTracker::VarMap::iterator VarTracker::GetLiveVar(const PP_Var& var) {
  return live_vars_.find(static_cast<int32>(var.value.as_id));
}

VarTracker::VarMap::const_iterator VarTracker::GetLiveVar(
    const PP_Var& var) const {
  return live_vars_.find(static_cast<int32>(var.value.as_id));
}

bool VarTracker::IsVarTypeRefcounted(PP_VarType type) const {
  return type == PP_VARTYPE_STRING || type == PP_VARTYPE_OBJECT;
}

void VarTracker::TrackedObjectGettingOneRef(VarMap::const_iterator obj) {
  // Anybody using tracked objects should override this.
  NOTREACHED();
}

void VarTracker::ObjectGettingZeroRef(VarMap::iterator iter) {
  DeleteObjectInfoIfNecessary(iter);
}

bool VarTracker::DeleteObjectInfoIfNecessary(VarMap::iterator iter) {
  if (iter->second.ref_count != 0 ||
      iter->second.track_with_no_reference_count != 0)
    return false;  // Object still alive.
  live_vars_.erase(iter);
  return true;
}

}  // namespace ppapi
