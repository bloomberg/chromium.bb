/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_WEAK_REF_CALL_ON_MAIN_THREAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_WEAK_REF_CALL_ON_MAIN_THREAD_H_

#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "native_client/src/include/nacl_scoped_ptr.h"

#include "native_client/src/third_party/ppapi/c/pp_errors.h"  // for PP_OK
#include "native_client/src/third_party/ppapi/cpp/completion_callback.h"  // for pp::CompletionCallback
#include "native_client/src/third_party/ppapi/cpp/core.h"  // for pp::
#include "native_client/src/third_party/ppapi/cpp/module.h"  // for pp::Module

namespace plugin {

// A typesafe utility to schedule a completion callback using weak
// references.  The callback function callback_fn is invoked
// regardless of whether the anchor has been abandoned, since
// callback_fn takes a WeakRef<R>* as argument.  The intention is that
// such callbacks, even deprived of any of its arguments (which has
// been deleted), may wish to do some cleanup / log a message.
template <typename R> bool WeakRefCallOnMainThread(
    nacl::WeakRefAnchor* anchor,
    int32_t delay_in_milliseconds,
    void callback_fn(nacl::WeakRef<R>* weak_data, int32_t err),
    R* raw_data) {
  nacl::WeakRef<R>* wp = anchor->MakeWeakRef<R>(raw_data);
  if (wp == NULL) {
    return false;
  }
  pp::CompletionCallback cc(reinterpret_cast<void (*)(void*, int32_t)>(
                                callback_fn),
                            reinterpret_cast<void*>(wp));
  pp::Module::Get()->core()->CallOnMainThread(
      delay_in_milliseconds,
      cc,
      PP_OK);
  return true;
}

template <typename R> class WeakRefAutoAbandonWrapper {
 public:
  WeakRefAutoAbandonWrapper(void (*callback_fn)(R* raw_data,
                                                int32_t err),
                            R* raw_data)
      : orig_callback_fn(callback_fn),
        orig_data(raw_data) {}

  void (*orig_callback_fn)(R* raw_data, int32_t err);
  nacl::scoped_ptr<R> orig_data;
};

template <typename R> void WeakRefAutoAbandoner(
    nacl::WeakRef<WeakRefAutoAbandonWrapper<R> >* wr,
    int32_t err) {
  nacl::scoped_ptr<WeakRefAutoAbandonWrapper<R> > p;
  wr->ReleaseAndUnref(&p);
  if (p == NULL) {
    NaClLog(4, "WeakRefAutoAbandoner: weak ref NULL, anchor was abandoned\n");
    return;
  }
  NaClLog(4, "WeakRefAutoAbandoner: weak ref okay, invoking callback\n");
  (*p->orig_callback_fn)(p->orig_data.get(), err);
  return;
}

// A typesafe utility to schedule a completion callback using weak
// references.  The callback function raw_callbac_fn takes an R* as
// argument, and is not invoked if the anchor has been abandoned.
template <typename R> bool WeakRefCallOnMainThread(
    nacl::WeakRefAnchor* anchor,
    int32_t delay_in_milliseconds,
    void raw_callback_fn(R* raw_data, int32_t err),
    R* raw_data) {

  nacl::WeakRef<WeakRefAutoAbandonWrapper<R> >* wp =
      anchor->MakeWeakRef<WeakRefAutoAbandonWrapper<R> >(
          new WeakRefAutoAbandonWrapper<R>(raw_callback_fn, raw_data));
  if (wp == NULL) {
    return false;
  }
  void (*WeakRefAutoAbandonerPtr)(
      nacl::WeakRef<WeakRefAutoAbandonWrapper<R> >* wr,
      int32_t err) = WeakRefAutoAbandoner<R>;
  pp::CompletionCallback cc(
      reinterpret_cast<void (*)(void*, int32_t)>(WeakRefAutoAbandonerPtr),
      reinterpret_cast<void*>(wp));
  pp::Module::Get()->core()->CallOnMainThread(
      delay_in_milliseconds,
      cc,
      PP_OK);
  return true;
}

}  // namespace plugin

#endif
