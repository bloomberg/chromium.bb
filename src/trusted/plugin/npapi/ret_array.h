/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_RET_ARRAY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_RET_ARRAY_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace plugin {

// RetArray is the struct used to encapsulate the return value from invoking
// a method from JavaScript.  Invoking a method with multiple return values
// returns a RetArray object, each of whose elements is the corresponding
// return value from the SRPC type signature.
class RetArray : public NPObject {
 public:
  explicit RetArray(NPP npp);
  ~RetArray();

  void SetAt(int index, NPVariant* value);

  // initializes NPVariant pointed by the argument to point to the same object
  bool ExportVariant(NPVariant* copy);
  NPObject* ExportObject();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RetArray);
  NPP npp_;
  NPVariant array_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_RET_ARRAY_H_
