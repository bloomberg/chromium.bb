// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_HOST_VAR_TRACKER_H_
#define WEBKIT_PLUGINS_PPAPI_HOST_VAR_TRACKER_H_

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "webkit/plugins/webkit_plugins_export.h"

typedef struct NPObject NPObject;

namespace ppapi {
class NPObjectVar;
class Var;
}

namespace webkit {
namespace ppapi {

// Adds NPObject var tracking to the standard PPAPI VarTracker for use in the
// renderer.
class HostVarTracker : public ::ppapi::VarTracker {
 public:
  HostVarTracker();
  virtual ~HostVarTracker();

  // Tracks all live NPObjectVar. This is so we can map between instance +
  // NPObject and get the NPObjectVar corresponding to it. This Add/Remove
  // function is called by the NPObjectVar when it is created and
  // destroyed.
  void AddNPObjectVar(::ppapi::NPObjectVar* object_var);
  void RemoveNPObjectVar(::ppapi::NPObjectVar* object_var);

  // Looks up a previously registered NPObjectVar for the given NPObject and
  // instance. Returns NULL if there is no NPObjectVar corresponding to the
  // given NPObject for the given instance. See AddNPObjectVar above.
  ::ppapi::NPObjectVar* NPObjectVarForNPObject(PP_Instance instance,
                                               NPObject* np_object);

  // Returns the number of NPObjectVar's associated with the given instance.
  // Returns 0 if the instance isn't known.
  WEBKIT_PLUGINS_EXPORT int GetLiveNPObjectVarsForInstance(
      PP_Instance instance) const;

  // Forcibly deletes all np object vars for the given instance. Used for
  // instance cleanup.
  void ForceFreeNPObjectsForInstance(PP_Instance instance);

 private:
  typedef std::map<NPObject*, ::ppapi::NPObjectVar*> NPObjectToNPObjectVarMap;

  // Lists all known NPObjects, first indexed by the corresponding instance,
  // then by the NPObject*. This allows us to look up an NPObjectVar given
  // these two pieces of information.
  //
  // The instance map is lazily managed, so we'll add the
  // NPObjectToNPObjectVarMap lazily when the first NPObject var is created,
  // and delete it when it's empty.
  typedef std::map<PP_Instance, linked_ptr<NPObjectToNPObjectVarMap> >
      InstanceMap;
  InstanceMap instance_map_;

  DISALLOW_COPY_AND_ASSIGN(HostVarTracker);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_HOST_VAR_TRACKER_H_
