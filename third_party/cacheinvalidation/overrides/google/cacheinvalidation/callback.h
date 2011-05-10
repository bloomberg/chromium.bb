// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_CACHEINVALIDATION_CALLBACK_H_
#define GOOGLE_CACHEINVALIDATION_CALLBACK_H_

#include "base/callback_old.h"

#define INVALIDATION_CALLBACK1_TYPE(Arg1) ::Callback1<Arg1>::Type

// Below are a collection of types and functions to make up for the
// limited capabilities of Chrome's callback.h.

namespace invalidation {

typedef ::Callback0::Type Closure;

static inline void DoNothing() {}

template <class T>
bool IsCallbackRepeatable(const T* callback) {
  return true;
}

namespace internal {

// Identity<T>::type is a typedef of T. Useful for preventing the
// compiler from inferring the type of an argument in templates.
template <typename T>
struct Identity {
  typedef T type;
};

// Specified by TR1 [4.7.2]
template <typename T> struct remove_reference {
  typedef T type;
};

template <typename T> struct remove_reference<T&> {
  typedef T type;
};

}  // namespace internal

// base/callback.h already handles partially applying member functions
// to an object, so just route to that. (We only need the one-arg
// case.)

template <class T, typename Arg1>
typename Callback1<Arg1>::Type* NewPermanentCallback(
    T* object, void (T::*method)(Arg1)) {
  return new CallbackImpl<T, void (T::*)(Arg1), Tuple1<Arg1> >(object, method);
}

// Define function runners for the partial application combinations
// that we need.

class ZeroArgFunctionRunner : public CallbackRunner<Tuple0> {
 public:
  ZeroArgFunctionRunner(void (*fn)()) : fn_(fn) {}

  virtual ~ZeroArgFunctionRunner() {}

  virtual void RunWithParams(const Tuple0& params) { fn_(); }

 private:
  void (*fn_)();
};

template <class T>
class ZeroArgMethodRunner : public CallbackRunner<Tuple0> {
 public:
  ZeroArgMethodRunner(T* obj, void (T::*meth)()) : obj_(obj), meth_(meth) {}

  virtual ~ZeroArgMethodRunner() {}

  virtual void RunWithParams(const Tuple0& params) {
    (obj_->*meth_)();
  }

 private:
  T* obj_;
  void (T::*meth_)();
};

template <class T, typename Arg1>
class OneArgCallbackRunner : public CallbackRunner<Tuple0> {
 public:
  OneArgCallbackRunner(T* obj, void (T::*meth)(Arg1),
                       Arg1 arg1)
      : obj_(obj), meth_(meth), arg1_(arg1) {}

  virtual ~OneArgCallbackRunner() {}

  virtual void RunWithParams(const Tuple0& params) {
    (obj_->*meth_)(arg1_);
  }

 private:
  T* obj_;
  void (T::*meth_)(Arg1);
  typename internal::remove_reference<Arg1>::type arg1_;
};

template <typename Arg1, typename Arg2>
class TwoArgFunctionRunner : public CallbackRunner<Tuple0> {
 public:
  TwoArgFunctionRunner(void (*fn)(Arg1, Arg2), Arg1 arg1, Arg2 arg2)
      : fn_(fn), arg1_(arg1), arg2_(arg2) {}

  virtual ~TwoArgFunctionRunner() {}

  virtual void RunWithParams(const Tuple0& params) { fn_(arg1_, arg2_); }

 private:
  void (*fn_)(Arg1, Arg2);
  typename internal::remove_reference<Arg1>::type arg1_;
  typename internal::remove_reference<Arg2>::type arg2_;
};

template <class T, typename Arg1, typename Arg2>
class TwoArgCallbackRunner : public CallbackRunner<Tuple0> {
 public:
  TwoArgCallbackRunner(T* obj, void (T::*meth)(Arg1, Arg2),
                       Arg1 arg1, Arg2 arg2)
      : obj_(obj), meth_(meth), arg1_(arg1), arg2_(arg2) {}

  virtual ~TwoArgCallbackRunner() {}

  virtual void RunWithParams(const Tuple0& params) {
    (obj_->*meth_)(arg1_, arg2_);
  }

 private:
  T* obj_;
  void (T::*meth_)(Arg1, Arg2);
  typename internal::remove_reference<Arg1>::type arg1_;
  typename internal::remove_reference<Arg2>::type arg2_;
};

template <class T, typename Arg1, typename Arg2>
class TwoArgOnePartialCallbackRunner : public CallbackRunner<Tuple1<Arg2> > {
 public:
  TwoArgOnePartialCallbackRunner(T* obj, void (T::*meth)(Arg1, Arg2),
                                 Arg1 arg1)
      : obj_(obj), meth_(meth), arg1_(arg1) {}

  virtual ~TwoArgOnePartialCallbackRunner() {}

  virtual void RunWithParams(const Tuple1<Arg2>& params) {
    (obj_->*meth_)(arg1_, params.a);
  }

 private:
  T* obj_;
  void (T::*meth_)(Arg1, Arg2);
  typename internal::remove_reference<Arg1>::type arg1_;
};

template <class T, typename Arg1, typename Arg2, typename Arg3>
class ThreeArgCallbackRunner : public CallbackRunner<Tuple0> {
 public:
  ThreeArgCallbackRunner(T* obj, void (T::*meth)(Arg1, Arg2, Arg3),
                         Arg1 arg1, Arg2 arg2, Arg3 arg3)
      : obj_(obj), meth_(meth), arg1_(arg1), arg2_(arg2), arg3_(arg3) {}

  virtual ~ThreeArgCallbackRunner() {}

  virtual void RunWithParams(const Tuple0& params) {
    (obj_->*meth_)(arg1_, arg2_, arg3_);
  }

 private:
  T* obj_;
  void (T::*meth_)(Arg1, Arg2, Arg3);
  typename internal::remove_reference<Arg1>::type arg1_;
  typename internal::remove_reference<Arg2>::type arg2_;
  typename internal::remove_reference<Arg3>::type arg3_;
};

// Then route the appropriate overloads of NewPermanentCallback() to
// use the above.

inline Callback0::Type* NewPermanentCallback(void (*fn)()) {
  return new ZeroArgFunctionRunner(fn);
}

template <class T1, class T2>
typename Callback0::Type* NewPermanentCallback(
    T1* object, void (T2::*method)()) {
  return new ZeroArgMethodRunner<T1>(object, method);
}

template <class T1, class T2, typename Arg1>
typename Callback0::Type* NewPermanentCallback(
    T1* object,
    void (T2::*method)(Arg1),
    typename internal::Identity<Arg1>::type arg1) {
  return new OneArgCallbackRunner<T1, Arg1>(object, method, arg1);
}

template <typename Arg1, typename Arg2>
Callback0::Type* NewPermanentCallback(
    void (*fn)(Arg1, Arg2),
    typename internal::Identity<Arg1>::type arg1,
    typename internal::Identity<Arg2>::type arg2) {
  return new TwoArgFunctionRunner<Arg1, Arg2>(fn, arg1, arg2);
}

template <class T1, class T2, typename Arg1, typename Arg2>
typename Callback0::Type* NewPermanentCallback(
    T1* object,
    void (T2::*method)(Arg1, Arg2),
    typename internal::Identity<Arg1>::type arg1,
    typename internal::Identity<Arg2>::type arg2) {
  return new TwoArgCallbackRunner<T1, Arg1, Arg2>(object, method, arg1, arg2);
}

template <class T1, class T2, typename Arg1, typename Arg2>
typename Callback1<Arg2>::Type* NewPermanentCallback(
    T1* object,
    void (T2::*method)(Arg1, Arg2),
    typename internal::Identity<Arg1>::type arg1) {
  return new TwoArgOnePartialCallbackRunner<T1, Arg1, Arg2>(
      object, method, arg1);
}

template <class T1, class T2, typename Arg1, typename Arg2, typename Arg3>
typename Callback0::Type* NewPermanentCallback(
    T1* object,
    void (T2::*method)(Arg1, Arg2, Arg3),
    typename internal::Identity<Arg1>::type arg1,
    typename internal::Identity<Arg2>::type arg2,
    typename internal::Identity<Arg3>::type arg3) {
  return new ThreeArgCallbackRunner<T1, Arg1, Arg2, Arg3>
      (object, method, arg1, arg2, arg3);
}

}  // namespace invalidation

#endif  // GOOGLE_CACHEINVALIDATION_CALLBACK_H_
