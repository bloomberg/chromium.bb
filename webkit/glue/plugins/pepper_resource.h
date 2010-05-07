// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

typedef struct _pp_Resource PP_Resource;

namespace pepper {

class DeviceContext2D;
class ImageData;
class PluginModule;

class Resource : public base::RefCountedThreadSafe<Resource> {
 public:
  explicit Resource(PluginModule* module);
  virtual ~Resource();

  PP_Resource GetResource() const;

  PluginModule* module() const { return module_; }

  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type.
  virtual DeviceContext2D* AsDeviceContext2D() { return NULL; }
  virtual ImageData* AsImageData() { return NULL; }

 private:
  PluginModule* module_;  // Non-owning pointer to our module.

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

// Returns a "NULL" resource. This is just a helper function so callers
// can avoid creating a resource with a 0 ID.
PP_Resource NullPPResource();

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_
