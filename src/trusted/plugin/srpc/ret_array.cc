/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdlib.h>
#include <string.h>
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/ret_array.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

// TODO(sehr): this whole module should probably be replaced by
// a call to javascript eval to create an array object.
//
namespace nacl_srpc {

// Class variable definitions.
int RetArray::number_alive_counter = 0;


RetArray::RetArray(NPP npp) : npp_(npp) {
  dprintf(("RetArray::RetArray(%p, %d)\n",
           static_cast<void *>(this),
           ++number_alive_counter));

  NPObject* window;
  NPN_GetValue(npp, NPNVWindowNPObject, &window);

  NPString script;
  script.UTF8Characters = "new Array();";
  script.UTF8Length = strlen(script.UTF8Characters);

  if (!NPN_Evaluate(npp, window, &script, &array_) ||
      !NPVARIANT_IS_OBJECT(array_)) {
    // something failed - nothing to do since we haven't created an object yet
  }
  NPN_ReleaseObject(window);
}

void RetArray::SetAt(int index, NPVariant *value) {
  NPN_SetProperty(npp_,
                  NPVARIANT_TO_OBJECT(array_),
                  NPN_GetIntIdentifier(index),
                  value);
}

RetArray::~RetArray() {
  dprintf(("RetArray::~RetArray(%p, %d)\n",
           static_cast<void *>(this),
           --number_alive_counter));
  NPN_ReleaseVariantValue(&array_);
}

bool RetArray::ExportVariant(NPVariant *copy) {
  NPN_RetainObject(NPVARIANT_TO_OBJECT(array_));
  memcpy(copy, &array_, sizeof(NPVariant));
  return true;
  //return &array_;
}

NPObject* RetArray::ExportObject() {
  return NPN_RetainObject(NPVARIANT_TO_OBJECT(array_));
}


}  // namespace nacl_srpc
