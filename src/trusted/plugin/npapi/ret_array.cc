/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdlib.h>
#include <string.h>
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/trusted/plugin/npapi/ret_array.h"
#include "native_client/src/trusted/plugin/utility.h"

using nacl::assert_cast;

namespace plugin {

RetArray::RetArray(NPP npp) : npp_(npp) {
  PLUGIN_PRINTF(("RetArray::RetArray(%p)\n", static_cast<void*>(this)));

  NPObject* window;
  NPN_GetValue(npp_, NPNVWindowNPObject, &window);

  NPString script;
  script.UTF8Characters = "new Array();";
  script.UTF8Length = assert_cast<uint32_t>(strlen(script.UTF8Characters));

  if (!NPN_Evaluate(npp_, window, &script, &array_) ||
      !NPVARIANT_IS_OBJECT(array_)) {
    // something failed - nothing to do since we haven't created an object yet
  }
  NPN_ReleaseObject(window);
}

void RetArray::SetAt(int index, NPVariant* value) {
  NPN_SetProperty(npp_,
                  NPVARIANT_TO_OBJECT(array_),
                  NPN_GetIntIdentifier(index),
                  value);
}

RetArray::~RetArray() {
  PLUGIN_PRINTF(("RetArray::~RetArray(%p)\n", static_cast<void*>(this)));
  NPN_ReleaseVariantValue(&array_);
}

bool RetArray::ExportVariant(NPVariant* copy) {
  NPN_RetainObject(NPVARIANT_TO_OBJECT(array_));
  memcpy(copy, &array_, sizeof(NPVariant));
  return true;
}

NPObject* RetArray::ExportObject() {
  return NPN_RetainObject(NPVARIANT_TO_OBJECT(array_));
}


}  // namespace plugin
