/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
