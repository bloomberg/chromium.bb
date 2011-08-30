// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_plugin_utils.h"

namespace remoting {

NPNetscapeFuncs* g_npnetscape_funcs = NULL;

std::string StringFromNPIdentifier(NPIdentifier identifier) {
  if (!g_npnetscape_funcs->identifierisstring(identifier))
    return std::string();
  NPUTF8* np_string = g_npnetscape_funcs->utf8fromidentifier(identifier);
  std::string string(np_string);
  g_npnetscape_funcs->memfree(np_string);
  return string;
}

std::string StringFromNPVariant(const NPVariant& variant) {
  if (!NPVARIANT_IS_STRING(variant))
    return std::string();
  const NPString& np_string = NPVARIANT_TO_STRING(variant);
  return std::string(np_string.UTF8Characters, np_string.UTF8Length);
}

NPVariant NPVariantFromString(const std::string& val) {
  size_t len = val.length();
  NPUTF8* chars =
      reinterpret_cast<NPUTF8*>(g_npnetscape_funcs->memalloc(len + 1));
  strcpy(chars, val.c_str());
  NPVariant variant;
  STRINGN_TO_NPVARIANT(chars, len, variant);
  return variant;
}

NPObject* ObjectFromNPVariant(const NPVariant& variant) {
  if (!NPVARIANT_IS_OBJECT(variant))
    return NULL;
  return NPVARIANT_TO_OBJECT(variant);
}

ScopedRefNPObject::ScopedRefNPObject() : object_(NULL) { }

ScopedRefNPObject::ScopedRefNPObject(NPObject* object)
    : object_(NULL) {
  *this = object;
}

ScopedRefNPObject::~ScopedRefNPObject() {
  *this = NULL;
}

ScopedRefNPObject& ScopedRefNPObject::operator=(NPObject* object) {
  if (object) {
    g_npnetscape_funcs->retainobject(object);
  }
  if (object_) {
    g_npnetscape_funcs->releaseobject(object_);
  }
  object_ = object;
  return *this;
}

}  // namespace remoting
