// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_HOST_PLUGIN_UTILS_H_
#define REMOTING_HOST_PLUGIN_HOST_PLUGIN_UTILS_H_

#include <string>

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace remoting {

// Global netscape functions initialized in NP_Initialize.
extern NPNetscapeFuncs* g_npnetscape_funcs;

// Convert an NPIdentifier into a std::string.
std::string StringFromNPIdentifier(NPIdentifier identifier);

// Convert an NPVariant into a std::string.
std::string StringFromNPVariant(const NPVariant& variant);

// Convert a std::string into an NPVariant.
// Caller is responsible for making sure that NPN_ReleaseVariantValue is
// called on returned value.
NPVariant NPVariantFromString(const std::string& val);

// Convert an NPVariant into an NSPObject.
NPObject* ObjectFromNPVariant(const NPVariant& variant);

// Scoped reference pointer for NPObjects. All objects using this class
// must be owned by g_npnetscape_funcs.
class ScopedRefNPObject {
 public:
  ScopedRefNPObject();
  explicit ScopedRefNPObject(NPObject* object);
  ~ScopedRefNPObject();

  // Release the held reference and replace it with |object|, incrementing
  // its reference count.
  ScopedRefNPObject& operator=(NPObject* object);

  NPObject* get() { return object_; }

 private:
  NPObject* object_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRefNPObject);
};

}  // namespace remoting

#endif  // REMOTING_HOST_PLUGIN_HOST_PLUGIN_UTILS_H_
