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
  script.utf8characters = "new Array();";
  script.utf8length = strlen(script.utf8characters);

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
