/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <assert.h>

#include "native_client/src/trusted/plugin/ppapi/var_utils.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability_io.h"

namespace plugin {

// TODO(polina): see if this can be moved to ppapi/cpp/var.h::Var
nacl::string VarToString(const pp::Var& var) {
  char buf[256];
  if (var.is_void())
    SNPRINTF(buf, sizeof buf, "Var<VOID>");
  else if (var.is_null())
    SNPRINTF(buf, sizeof buf, "Var<NULL>");
  else if (var.is_bool())
    SNPRINTF(buf, sizeof buf, var.AsBool() ? "Var<true>" : "Var<false>");
  else if (var.is_int())
    SNPRINTF(buf, sizeof buf, "Var<%"NACL_PRIu32">", var.AsInt());
  else if (var.is_double())
    SNPRINTF(buf, sizeof buf, "Var<%f>", var.AsDouble());
  else if (var.is_string())
    SNPRINTF(buf, sizeof buf, "Var<'%s'>", var.AsString().c_str());
  else if (var.is_object())
    SNPRINTF(buf, sizeof buf, "Var<OBJECT>");
  return buf;
}


void PPVarToNaClSrpcArg(const pp::Var& var, NaClSrpcArg* arg) {
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
      assert(var.is_bool());
      arg->u.bval = var.AsBool();
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      assert(var.is_double());
      arg->u.dval = var.AsDouble();
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      assert(var.is_int());
      arg->u.ival = var.AsInt();
      break;
    case NACL_SRPC_ARG_TYPE_STRING: {
      assert(var.is_string());
      nacl::string var_as_string = var.AsString();
      int size = var_as_string.size();
      arg->u.sval = reinterpret_cast<char*>(malloc(size + 1));
      SNPRINTF(arg->u.sval, sizeof arg->u.sval, "%s", var_as_string.c_str());
      break;
    }
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    case NACL_SRPC_ARG_TYPE_HANDLE:
    case NACL_SRPC_ARG_TYPE_OBJECT:
      // TODO(polina): support marshalling to srpc arrays, handles and objects
      NACL_UNIMPLEMENTED();
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      break;
  }
}


pp::Var NaClSrpcArgToPPVar(const NaClSrpcArg* arg) {
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
      return pp::Var(arg->u.bval != 0);
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      return pp::Var(arg->u.dval);
    case NACL_SRPC_ARG_TYPE_INT:
      return pp::Var(arg->u.ival);
    case NACL_SRPC_ARG_TYPE_STRING:
      return pp::Var(arg->u.sval);
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    case NACL_SRPC_ARG_TYPE_HANDLE:
    case NACL_SRPC_ARG_TYPE_OBJECT:
      // TODO(polina): support marshalling from srpc arrays, handles and objects
      NACL_UNIMPLEMENTED();
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      break;
  }
  return pp::Var();
}


}  // namespace plugin
