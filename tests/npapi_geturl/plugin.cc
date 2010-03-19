// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/npapi_runtime/plugin.h"

#include <nacl/nacl_npapi.h>
#include <nacl/npupp.h>
#include <string>

namespace {

static NPIdentifier* properties = NULL;

static const NPUTF8* propertyStrings[] = {
  "__moduleReady",
};

static const int propertyCount =
    sizeof(propertyStrings) / sizeof(propertyStrings[0]);

enum {
  kModuleReadyIdent = 0
};


static NPIdentifier* identifiers = NULL;

static const NPUTF8* identifierStrings[] = {
  "loadUrl",
};

static const int identifierCount =
    sizeof(identifierStrings) / sizeof(identifierStrings[0]);

enum {
  kLoadUrlIdent = 0
};


void InitializeProperties() {
  // If this has already been run, return.
  if (NULL != properties) {
    return;
  }
  // Allocate the properties array.
  properties = new NPIdentifier[propertyCount];
  if (NULL == properties) {
    return;
  }
  // Populate the properties array.
  browser->getstringidentifiers(propertyStrings,
                                propertyCount,
                                properties);
}

void InitializeIdentifiers() {
  // If this has already been run, return.
  if (NULL != identifiers) {
    return;
  }
  // Allocate the identifiers array.
  identifiers = new NPIdentifier[identifierCount];
  if (NULL == identifiers) {
    return;
  }
  // Populate the identifiers array.
  browser->getstringidentifiers(identifierStrings,
                                identifierCount,
                                identifiers);
}

static NPVariant* CopyNPVariant(const NPVariant* from) {
  NPVariant* copy = new NPVariant;
  if (NULL == copy) {
    return NULL;
  }
  *copy = *from;
  if (NPVARIANT_IS_OBJECT(*copy)) {
    // Increase the ref count.
    browser->retainobject(NPVARIANT_TO_OBJECT(*copy));
  } else if (NPVARIANT_IS_STRING(*copy)) {
    // Copy the string.
    NPUTF8* str = new NPUTF8[NPVARIANT_TO_STRING(*from).UTF8Length];
    if (NULL == str) {
      delete copy;
    }
    memcpy(str,
           NPVARIANT_TO_STRING(*from).UTF8Characters,
           NPVARIANT_TO_STRING(*from).UTF8Length);
    NPVARIANT_TO_STRING(*copy).UTF8Characters = str;
  }
  return copy;
}

static char* StringToChars(NPString string) {
  // Some browsers' length includes the '\0', others' do not.
  // Therefore we need to build a string that has a '\0' afterwards.
  NPUTF8* str = new NPUTF8[string.UTF8Length + 1];
  if (NULL == str) {
    return NULL;
  }
  memcpy(str, string.UTF8Characters, string.UTF8Length);
  str[string.UTF8Length] = '\0';
  return str;
}
}  // namespace

Plugin* Plugin::New(NPP instance,
                    NPMIMEType mime_type,
                    int16_t argc,
                    char* argn[],
                    char* argv[]) {
  InitializeProperties();
  InitializeIdentifiers();
  // Create the scriptable object.
  NPObject* object = browser->createobject(instance, GetNPSimpleClass());
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  if (NULL == plugin) {
    return NULL;
  }
  // Set the attributes on the plugin.
  plugin->instance_ = instance;
  plugin->mime_type_ = strdup(mime_type);
  plugin->argc_ = argc;
  for (int16_t i = 0; i < plugin->argc_; ++i) {
    plugin->argn_to_argv_[strdup(argn[i])] = strdup(argv[i]);
  }
  // The plugin is ready to be used.
  NPVariant one;
  INT32_TO_NPVARIANT(1, one);
  plugin->setProperty(properties[kModuleReadyIdent], &one);

  return plugin;
}

void Plugin::invalidate() {
  // Do nothing.
}

bool Plugin::hasMethod(NPIdentifier methodName) {
  for (int i = 0; i < identifierCount; ++i) {
    if (identifiers[i] == methodName) {
      return true;
    }
  }
  return false;
}

bool Plugin::invoke(NPIdentifier methodName,
                    const NPVariant *args,
                    uint32_t argCount,
                    NPVariant *result) {
  if (identifiers[kLoadUrlIdent] == methodName &&
      2 == argCount &&
      NPVARIANT_IS_STRING(args[0]) &&
      NPVARIANT_IS_STRING(args[1])) {
    const char* url = StringToChars(NPVARIANT_TO_STRING(args[0]));
    const char* window = StringToChars(NPVARIANT_TO_STRING(args[1]));
    return NPN_GetURL(instance_, url, window);
  }
  return false;
}

bool Plugin::invokeDefault(const NPVariant *args,
                           uint32_t argCount,
                           NPVariant *result) {
  if (1 != argCount || NULL == result) {
    return false;
  }
  // Return a copy of the parameter as the return value.
  NPVariant* copy = CopyNPVariant(&args[0]);
  if (NULL == copy) {
    return false;
  }
  *result = *copy;
  return true;
}

bool Plugin::hasProperty(NPIdentifier propertyName) {
  std::map<NPIdentifier, const NPVariant*>::iterator i =
      properties_.find(propertyName);
  return properties_.end() != i;
}

bool Plugin::getProperty(NPIdentifier propertyName,
                         NPVariant *result) {
  std::map<NPIdentifier, const NPVariant*>::iterator i =
      properties_.find(propertyName);
  if (properties_.end() == i) {
    return false;
  }
  *result = *(i->second);
  return true;
}

bool Plugin::setProperty(NPIdentifier propertyName,
                         const NPVariant* value) {
  std::map<NPIdentifier, const NPVariant*>::iterator i =
      properties_.find(propertyName);
  if (properties_.end() != i) {
    // Decrement the refcount on the previously held object.
    browser->releasevariantvalue(const_cast<NPVariant*>(i->second));
    // Release the copy of the previously value.
    delete i->second;
  }
  // Create a copy of the variant.
  NPVariant* copy = CopyNPVariant(value);
  if (NULL == copy) {
    return false;
  }
  properties_[propertyName] = copy;
  return true;
}

bool Plugin::removeProperty(NPIdentifier propertyName) {
  std::map<NPIdentifier, const NPVariant*>::iterator i =
      properties_.find(propertyName);
  if (properties_.end() == i) {
    return false;
  }
  // Decrement the refcount on the object.
  browser->releasevariantvalue(const_cast<NPVariant*>(i->second));
  // Release the copy.
  delete i->second;
  // Remove the association.
  properties_.erase(propertyName);
  return true;
}

bool Plugin::enumerate(NPIdentifier** name,
                       uint32_t* count) {
  return false;
}

bool Plugin::construct(const NPVariant* args,
                       uint32_t argCount,
                       NPVariant* result) {
  return false;
}

/* Forward declarations */
NPObject* Plugin::Allocate(NPP npp,
                           NPClass* npclass) {
  return reinterpret_cast<NPObject*>(new Plugin());
}

void Plugin::Deallocate(NPObject* object) {
  delete reinterpret_cast<Plugin*>(object);
}

void Plugin::Invalidate(NPObject* object) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->invalidate();
}

bool Plugin::HasMethod(NPObject* object,
                       NPIdentifier methodName) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->hasMethod(methodName);
}

bool Plugin::Invoke(NPObject* object,
                    NPIdentifier methodName,
                    const NPVariant *args,
                    uint32_t argCount,
                    NPVariant *result) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->invoke(methodName, args, argCount, result);
}

bool Plugin::InvokeDefault(NPObject *object,
                           const NPVariant *args,
                           uint32_t argCount,
                           NPVariant *result) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->invokeDefault(args, argCount, result);
}

bool Plugin::HasProperty(NPObject *object,
                         NPIdentifier propertyName) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->hasProperty(propertyName);
}

bool Plugin::GetProperty(NPObject *object,
                         NPIdentifier propertyName,
                         NPVariant *result) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->getProperty(propertyName, result);
}

bool Plugin::SetProperty(NPObject* object,
                         NPIdentifier propertyName,
                         const NPVariant* value) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->setProperty(propertyName, value);
}

bool Plugin::RemoveProperty(NPObject* object,
                            NPIdentifier propertyName) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->removeProperty(propertyName);
}

bool Plugin::Enumerate(NPObject* object,
                       NPIdentifier** value,
                       uint32_t* count) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->enumerate(value, count);
}

bool Plugin::Construct(NPObject* object,
                       const NPVariant* args,
                       uint32_t argCount,
                       NPVariant* result) {
  Plugin* plugin = reinterpret_cast<Plugin*>(object);
  return plugin->construct(args, argCount, result);
}

NPClass *Plugin::GetNPSimpleClass() {
  static NPClass npcRefClass = {
    NP_CLASS_STRUCT_VERSION,
    Allocate,
    Deallocate,
    Invalidate,
    HasMethod,
    Invoke,
    InvokeDefault,
    HasProperty,
    GetProperty,
    SetProperty,
    RemoveProperty,
    Enumerate,
    Construct
  };

  return &npcRefClass;
}
