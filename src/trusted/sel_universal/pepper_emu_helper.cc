/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>

#include "ppapi/c/pp_var.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

// This is not exactly like "struct PP_Var" so we do not pretend it is
// c.f.: src/shared/ppapi_proxy/object_serialize.cc
struct JSString {
  uint32_t js_type;
  uint32_t size;
  char     payload[1];
};

struct JSData {
  uint32_t js_type;
  uint32_t val;
};


std::string GetMarshalledJSString(NaClSrpcArg* arg) {
  JSString* data = reinterpret_cast<JSString*>(arg->arrays.carr);
  CHECK(data->js_type == PP_VARTYPE_STRING);
  return std::string(data->payload, data->size);
}

int GetMarshalledJSInt(NaClSrpcArg* arg) {
  JSData* data = reinterpret_cast<JSData*>(arg->arrays.carr);
  CHECK(data->js_type == PP_VARTYPE_INT32);
  return data->val;
}

bool GetMarshalledJSBool(NaClSrpcArg* arg) {
  JSData* data = reinterpret_cast<JSData*>(arg->arrays.carr);
  CHECK(data->js_type == PP_VARTYPE_BOOL);
  return data->val != 0;
}

void SetMarshalledJSInt(NaClSrpcArg* arg, int val) {
  JSData* data = reinterpret_cast<JSData*>(malloc(sizeof *data));
  data->js_type = PP_VARTYPE_INT32;
  data->val = val;

  arg->arrays.carr = reinterpret_cast<char*>(data);
  arg->u.count = sizeof *data;
}
