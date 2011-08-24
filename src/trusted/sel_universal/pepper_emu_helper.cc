/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>

#include "ppapi/c/pp_var.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/sel_universal/pepper_emu_helper.h"
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

// static
ResourceBase* ResourceBase::chain_ = NULL;

// static
// All handles will have 4 digits (or maybe more).
// We also start each new resource type at a new thousand boundary.
int ResourceBase::chain_handle_last_ = 1000;

// static
ResourceBase* ResourceBase::FindResource(int handle) {
  for (ResourceBase* res = chain_; res != NULL; res = res->next_) {
    if (res->first_handle_ <= handle &&
        handle < (res->first_handle_ + res->num_handles_)) {
      return res;
    }
  }
  return NULL;
}


static int RoundUp(int n, int r) {
  return (n + (r-1)) / r * r;
}

static bool IsUnused(int n) {
  return n <= 0;
}

ResourceBase::ResourceBase(int num_handles, const char* type)
  : next_(chain_),
    first_handle_(chain_handle_last_),
    num_handles_(num_handles),
    type_(type),
    used_(new int[num_handles]) {
  for (int i = 0; i < num_handles; ++i) {
    used_[i] = 0;
  }

  chain_handle_last_ = RoundUp(chain_handle_last_ + num_handles, 1000);
  chain_ = this;
}


ResourceBase::~ResourceBase() {
  delete [] used_;
}


bool ResourceBase::DecRefCount(int handle) {
  const int pos = handle - first_handle_;
  if (pos < 0 || pos >= num_handles_) return false;
  if (IsUnused(used_[pos])) return false;
  --used_[pos];
  return true;
}


bool ResourceBase::IncRefCount(int handle) {
  const int pos = handle - first_handle_;
  if (pos < 0 || pos >= num_handles_) return false;
  if (IsUnused(used_[pos])) return false;
  ++used_[pos];
  return true;
}


int ResourceBase::GetRefCount(int handle) {
  const int pos = handle - first_handle_;
  if (pos < 0 || pos >= num_handles_) return -1;
  return used_[pos];
}


int ResourceBase::Alloc() {
  for (int i = 0; i < num_handles_; ++i) {
    if (IsUnused(used_[i])) {
        used_[i] = 1;
        return first_handle_ + i;
      }
  }
  NaClLog(LOG_FATAL, "Cannot alloc resource %s\n", type_);
  return -1;
}
