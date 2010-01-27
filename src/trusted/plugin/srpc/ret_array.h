/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_RET_ARRAY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_RET_ARRAY_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl_srpc {

// RetArray is the struct used to encapsulate the return value from invoking
// a method from JavaScript.  Invoking a method with multiple return values
// returns a RetArray object, each of whose elements is the corresponding
// return value from the SRPC type signature.
class RetArray : public NPObject {
 public:
   explicit RetArray(NPP);
   ~RetArray();
   void SetAt(int index, NPVariant* value);
  // initializes NPVariant pointed by the argument to point to the same object
  bool ExportVariant(NPVariant* copy);
  NPObject* ExportObject();
  static int number_alive() { return number_alive_counter; }
 public:
  NPP npp_;
  NPVariant array_;

 private:
  static int number_alive_counter;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_RET_ARRAY_H_
