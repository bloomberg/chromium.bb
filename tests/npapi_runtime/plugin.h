// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_NPAPI_RUNTIME_PLUGIN_H_
#define NATIVE_CLIENT_TESTS_NPAPI_RUNTIME_PLUGIN_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nacl/nacl_npapi.h>
#include <nacl/npupp.h>
#include <map>

class Plugin {
 public:
  static Plugin* New(NPP instance,
                     NPMIMEType mime_type,
                     int16_t argc,
                     char* argn[],
                     char* argv[]);

  //  Get the npruntime function table.  Used during construction.
  static NPClass *GetNPSimpleClass();

 private:
  // The implementations of the npruntime methods.
  void invalidate();
  bool hasMethod(NPIdentifier methodName);
  bool invoke(NPIdentifier methodName,
              const NPVariant *args,
              uint32_t argCount,
              NPVariant *result);
  bool invokeDefault(const NPVariant *args,
                     uint32_t argCount,
                     NPVariant *result);
  bool hasProperty(NPIdentifier propertyName);
  bool getProperty(NPIdentifier propertyName,
                   NPVariant *result);
  bool setProperty(NPIdentifier name,
                   const NPVariant* value);
  bool removeProperty(NPIdentifier propertyName);
  bool enumerate(NPIdentifier** value,
                 uint32_t* count);
  bool construct(const NPVariant* args,
                 uint32_t argCount,
                 NPVariant* result);

  // The static methods used to populate the class tables.
  static NPObject* Allocate(NPP npp,
                            NPClass* npclass);
  static void Deallocate(NPObject* object);
  static void Invalidate(NPObject* object);
  static bool HasMethod(NPObject* object,
                        NPIdentifier methodName);
  static bool Invoke(NPObject* object,
                     NPIdentifier methodName,
                     const NPVariant *args,
                     uint32_t argCount,
                     NPVariant *result);
  static bool InvokeDefault(NPObject *object,
                            const NPVariant *args,
                            uint32_t argCount,
                            NPVariant *result);
  static bool HasProperty(NPObject *object,
                          NPIdentifier propertyName);
  static bool GetProperty(NPObject *object,
                          NPIdentifier propertyName,
                          NPVariant *result);
  static bool SetProperty(NPObject* object,
                          NPIdentifier propertyName,
                          const NPVariant* value);
  static bool RemoveProperty(NPObject *object,
                             NPIdentifier propertyName);
  static bool Enumerate(NPObject* object,
                        NPIdentifier** value,
                        uint32_t* count);
  static bool Construct(NPObject* object,
                      const NPVariant* args,
                      uint32_t argCount,
                      NPVariant* result);

  NPObject header_;
  NPP instance_;
  NPMIMEType mime_type_;
  int16_t argc_;
  std::map<char*, char*> argn_to_argv_;
  std::map<NPIdentifier, const NPVariant*> properties_;
};

extern NPNetscapeFuncs* browser;

#endif  // NATIVE_CLIENT_TESTS_NPAPI_RUNTIME_PLUGIN_H_
