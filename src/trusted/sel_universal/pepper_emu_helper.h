/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_LDR_UNIVERSAL_PEPPER_EMU_HELPER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_LDR_UNIVERSAL_PEPPER_EMU_HELPER_H_

#include <string>

const int kMaxIdsPerHandleType = 128;

// This template class associates ranges of resource handles
// actual data structures containing actual information about
// the handle.
// For now this extremely primitive - just good enough to handle
// url loader related task. It will likely signifantly change over time.
template<class Data> class Resource {
 public:
  Resource(int first_id, const std::string& name) :
  first_id_(first_id), name_(name) {
    // mark all as not used
    for (int i = 0; i < kMaxIdsPerHandleType; ++i) {
      used_[i] = false;
    }
  }

  // Release the given resource handle
  bool Release(int handle) {
    const int pos = handle - first_id_;
    if (pos < 0 || pos >= kMaxIdsPerHandleType) return false;
    if (!used_[pos]) return false;

    used_[pos] = false;
    return true;
  }

  // Allocate the the next free resource handle in range
  int Alloc() {
    for (int i = 0; i < kMaxIdsPerHandleType; ++i) {
      if (!used_[i]) {
        used_[i] = true;
        return first_id_ + i;
      }
    }
    return -1;
  }

  // Get pointer to data structure associated with handle
  Data* GetDataForHandle(int handle) {
    const int pos = handle - first_id_;
    if (pos < 0 || pos >= kMaxIdsPerHandleType) return NULL;
    if (!used_[pos]) {
      NaClLog(LOG_FATAL, "using released resource of type %s\n", name_.c_str());
      return NULL;
    }
    return &data_[pos];
  }

 private:
  Data data_[kMaxIdsPerHandleType];
  bool used_[kMaxIdsPerHandleType];
  int first_id_;
  std::string name_;
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
