/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_LDR_UNIVERSAL_PEPPER_EMU_HELPER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_LDR_UNIVERSAL_PEPPER_EMU_HELPER_H_

#include <string>

// The classes below associate ranges of resource handles with
// data structures containing actual information about
// the handle.
// This is still quite primitive and will change over time.
class ResourceBase {
 public:
  ResourceBase(int num_handles, const char* type);
  ~ResourceBase();
  const char* GetType() { return type_; }
  // Find a free handle (refcount == 0).
  int Alloc();
  // Get refcount for handle
  int GetRefCount(int handle);
  // Increment refcount of handle.
  bool IncRefCount(int handle);
  // Decrement refcount of handle,
  bool DecRefCount(int handle);

  static ResourceBase* FindResource(int handle);

 private:
  static ResourceBase* chain_;
  // Last used handle. This maybe rounded up - so there can be gaps).
  static int chain_handle_last_;

 protected:
  // Next ResourceBase in chain starting with chain_
  ResourceBase* next_;
  // First handle in this range.
  const int first_handle_;
  // Width of the range.
  const int num_handles_;
  // Resource type respresent as a string.
  const char* type_;
  // Refcount for each handle in the range - zero means unused.
  int* const used_;
};


template<class Data> class Resource : public ResourceBase {
 public:
  Resource(int num_handles, const char* type)
    : ResourceBase(num_handles, type), data_(new Data[num_handles]) {}

  ~Resource() {
    delete [] data_;
  }

  // Get pointer to data structure associated with handle
  Data* GetDataForHandle(int handle) {
    const int pos = handle - first_handle_;
    if (pos < 0 || pos >= num_handles_) return NULL;
    if (used_[pos] <= 0) {
      NaClLog(LOG_FATAL, "using released resource of type %s\n", type_);
      return NULL;
    }
    return &data_[pos];
  }

 private:
  Data* const data_;
};


struct NaClSrpcArg;
// When the a nexe needs to make ppapi calls via srpc,
// argument of type "struct PP_Var" are "srpc serialized".
// So the underlying value is wrapped twice.
// The helper functions below help with extracting and setting the actual value.
std::string GetMarshalledJSString(NaClSrpcArg* arg);
int GetMarshalledJSInt(NaClSrpcArg* arg);
bool GetMarshalledJSBool(NaClSrpcArg* arg);

void SetMarshalledJSInt(NaClSrpcArg* arg, int val);
#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_LDR_UNIVERSAL_PEPPER_EMU_HELPER_H_ */
