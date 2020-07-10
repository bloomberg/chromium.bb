// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_PROTOBUF_UTILS_H_
#define IPC_IPC_MESSAGE_PROTOBUF_UTILS_H_

#include "build/build_config.h"

#if defined(OS_NACL_NONSFI)
static_assert(false,
              "ipc_message_protobuf_utils is not able to work with "
              "nacl_nonsfi configuration.");
#endif

#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace IPC {

template <class RepeatedFieldLike, class StorageType>
struct RepeatedFieldParamTraits {
  typedef RepeatedFieldLike param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.size());
    for (int i = 0; i < p.size(); i++)
      WriteParam(m, p.Get(i));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    // ReadLength() checks for < 0 itself.
    if (!iter->ReadLength(&size))
      return false;
    // Avoid integer overflow / assertion failure in Reserve() function.
    if (INT_MAX / sizeof(StorageType) <= static_cast<size_t>(size))
      return false;
    r->Reserve(size);
    for (int i = 0; i < size; i++) {
      if (!ReadParam(m, iter, r->Add()))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (int i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam(p.Get(i), l);
    }
  }
};

template <class P>
struct ParamTraits<google::protobuf::RepeatedField<P>> :
    RepeatedFieldParamTraits<google::protobuf::RepeatedField<P>, P> {};

template <class P>
struct ParamTraits<google::protobuf::RepeatedPtrField<P>> :
    RepeatedFieldParamTraits<google::protobuf::RepeatedPtrField<P>, void*> {};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_PROTOBUF_UTILS_H_
