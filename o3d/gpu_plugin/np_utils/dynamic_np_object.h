// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_DYNAMIC_NP_OBJECT_H_
#define O3D_GPU_PLUGIN_NP_UTILS_DYNAMIC_NP_OBJECT_H_

#include <map>

#include "o3d/gpu_plugin/np_utils/base_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_utils.h"

namespace o3d {
namespace gpu_plugin {

// NPObjects of this type have a dictionary of property name / variant pairs
// that can be changed at runtime through NPAPI.
class DynamicNPObject : public BaseNPObject {
 public:
  explicit DynamicNPObject(NPP npp);

  virtual void Invalidate();
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant* result);
  virtual bool SetProperty(NPIdentifier name, const NPVariant* value);
  virtual bool RemoveProperty(NPIdentifier name);
  virtual bool Enumerate(NPIdentifier** names, uint32_t* count);

 private:
  typedef std::map<NPIdentifier, SmartNPVariant> PropertyMap;
  PropertyMap properties_;
  DISALLOW_COPY_AND_ASSIGN(DynamicNPObject);
};
}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_DYNAMIC_NP_OBJECT_H_
