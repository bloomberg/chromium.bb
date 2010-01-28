/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nacl/nacl_npapi.h>

/* Forward declarations */
static NPObject* Allocate(NPP npp, NPClass* npclass);
static void Deallocate(NPObject* object);
static void Invalidate(NPObject* object);
static bool HasMethod(NPObject* obj, NPIdentifier methodName);
static bool Invoke(NPObject* obj, NPIdentifier methodName,
                   const NPVariant *args, uint32_t argCount, NPVariant *result);
static bool InvokeDefault(NPObject *obj, const NPVariant *args,
                          uint32_t argCount, NPVariant *result);
static bool HasProperty(NPObject *obj, NPIdentifier propertyName);
static bool GetProperty(NPObject *obj, NPIdentifier propertyName,
                        NPVariant *result);
static bool SetProperty(NPObject* object,
                        NPIdentifier name,
                        const NPVariant* value);

/* Global Variables */
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
  SetProperty
};

static NPObject* Allocate(NPP npp, NPClass* npclass) {
  printf("*** Allocate\n");
  return (NPObject *)malloc(sizeof(NPObject));
}

static void Deallocate(NPObject* object) {
  printf("*** Dallocate\n");
  free(object);
}

static void Invalidate(NPObject* object) {
  printf("*** Invalidate\n");
}

/* Note that this HasMethod() always returns true, and below Invoke    */
/* always uses InvokeDefault which returns 42. The commented code      */
/* below shows how you would examine the method name in case you       */
/* wanted to have multiple methods with different behaviors.           */
static bool HasMethod(NPObject* obj, NPIdentifier methodName) {
  printf("*** HasMethod\n");
  /*  char *name = npnfuncs->utf8fromidentifier(methodName);  */
  /*  printf(("*** hasMethod(%s)\n", name)); */
  return true;
}

static bool InvokeDefault(NPObject *obj, const NPVariant *args,
                          uint32_t argCount, NPVariant *result) {
  printf("*** InvokeDefault\n");
  if (result) {
    INT32_TO_NPVARIANT(42, *result);
  }
  return true;
}

static bool InvokeHW(NPObject *obj, const NPVariant *args,
                     uint32_t argCount, NPVariant *result) {
  printf("*** InvokeHW\n");
  if (result) {
    const char *msg = "hello, world.";
    const int msglength = strlen(msg) + 1;
    /* Note: msgcopy will be freed downstream by NPAPI, so */
    /* it needs to be allocated here with NPN_MemAlloc.    */
    char *msgcopy = (char *)NPN_MemAlloc(msglength);
    strncpy(msgcopy, msg, msglength);
    STRINGN_TO_NPVARIANT(msgcopy, msglength - 1, *result);
  }
  return true;
}

/* Note that this Invoke() always calls InvokeDefault, which returns 42.    */
/* The commented code below shows how you would examine the method name     */
/* in case you wanted to have multiple methods with different behaviors.    */
static bool Invoke(NPObject* obj, NPIdentifier methodName,
                   const NPVariant *args, uint32_t argCount,
                   NPVariant *result) {
  char *name = NPN_UTF8FromIdentifier(methodName);
  bool rval = false;
  printf("*** Invoke(%s)\n", name);

  if (name && !strcmp((const char *)name, "helloworld")) {
    rval = InvokeHW(obj, args, argCount, result);
  } else if (name && !strcmp((const char *)name, "loopforever")) {
    /* for testing purposes only */
    while (1);
  } else {
    rval = InvokeDefault(obj, args, argCount, result);
  }
  /* Since name was allocated above by NPN_UTF8FromIdentifier,      */
  /* it needs to be freed here.                                     */
  NPN_MemFree(name);
  return rval;
}

static bool HasProperty(NPObject *obj, NPIdentifier propertyName) {
  printf("*** HasProperty(%p)\n", propertyName);
  if (NPN_IdentifierIsString(propertyName)) {
    printf("*** is a string\n");
    char *name = NPN_UTF8FromIdentifier(propertyName);
    printf("*** HasProperty(string %s)\n", name);
  } else {
    int i = NPN_IntFromIdentifier(propertyName);
    printf("*** HasProperty(int %d)\n", i);
  }
  return false;
}

static bool GetProperty(NPObject *obj, NPIdentifier propertyName,
                        NPVariant *result) {
  printf("*** GetProperty\n");
  return false;
}

static bool SetProperty(NPObject* object, NPIdentifier name,
                        const NPVariant* value) {
  printf("*** SetProperty\n");
  return false;
}

NPClass *GetNPSimpleClass() {
  return &npcRefClass;
}
