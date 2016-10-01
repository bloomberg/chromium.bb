// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_MakeCancellable_h
#define WTF_MakeCancellable_h

#include "base/logging.h"
#include "wtf/Functional.h"
#include "wtf/RefCounted.h"
#include "wtf/WTFExport.h"
#include <memory>

namespace WTF {

class ScopedFunctionCanceller;

namespace internal {

class WTF_EXPORT FunctionCanceller : public RefCounted<FunctionCanceller> {
 public:
  virtual void cancel() = 0;
  virtual bool isActive() const = 0;

 protected:
  FunctionCanceller();
  virtual ~FunctionCanceller();
  friend RefCounted<FunctionCanceller>;

  DISALLOW_COPY_AND_ASSIGN(FunctionCanceller);
};

template <typename... Params>
class FunctionCancellerImpl final : public FunctionCanceller {
 public:
  FunctionCancellerImpl(std::unique_ptr<Function<void(Params...)>> function)
      : m_function(std::move(function)) {
    DCHECK(m_function);
  }

  void runUnlessCancelled(const ScopedFunctionCanceller&, Params... params) {
    if (m_function)
      (*m_function)(std::forward<Params>(params)...);
  }

  void cancel() override { m_function = nullptr; }

  bool isActive() const override { return !!m_function; }

 private:
  ~FunctionCancellerImpl() override = default;

  std::unique_ptr<WTF::Function<void(Params...)>> m_function;

  DISALLOW_COPY_AND_ASSIGN(FunctionCancellerImpl);
};

}  // namespace internal

// ScopedFunctionCanceller is a handle associated to a Function, and cancels the
// invocation of the Function on the scope out or cancel() call.
// Example:
//   void Foo() {}
//
//   std::unique_ptr<Closure> f = bind(&Foo);
//   auto result = makeCancellable(std::move(f));
//
//   {
//       ScopedFunctionCanceller scopedCanceller = std::move(result.canceller);
//       // Scope out of |scopedCanceller| cancels Foo invocation.
//       // (*result.function)(); will be no-op.
//   }
//
//   ScopedFunctionCanceller scopedCanceller = std::move(result.canceller);
//
//   // Manual cancellation is also available. This cancels the invocation
//   // of Foo too.
//   scopedCanceller.cancel();
//
//   // detach() unassociates the FunctionCanceller instance without cancelling
//   // it. After detach() call, the destructor nor cancel() no longer cancels
//   // the invocation of Foo.
//   scopedCanceller.detach();
//
class WTF_EXPORT ScopedFunctionCanceller {
  DISALLOW_NEW();

 public:
  ScopedFunctionCanceller();
  explicit ScopedFunctionCanceller(PassRefPtr<internal::FunctionCanceller>);

  ScopedFunctionCanceller(ScopedFunctionCanceller&&);
  ScopedFunctionCanceller& operator=(ScopedFunctionCanceller&&);
  ScopedFunctionCanceller(const ScopedFunctionCanceller&) = delete;
  ScopedFunctionCanceller& operator=(const ScopedFunctionCanceller&) = delete;

  ~ScopedFunctionCanceller();
  void detach();
  void cancel();
  bool isActive() const;

 private:
  RefPtr<internal::FunctionCanceller> m_canceller;
};

template <typename... Params>
struct MakeCancellableResult {
  ScopedFunctionCanceller canceller;
  std::unique_ptr<Function<void(Params...)>> function;

  MakeCancellableResult(ScopedFunctionCanceller canceller,
                        std::unique_ptr<Function<void(Params...)>> function)
      : canceller(std::move(canceller)), function(std::move(function)) {}
};

// makeCancellable wraps a WTF::Function to make the function cancellable.
// This function returns a WTF::Function, and a ScopedFunctionCanceller.
// An invocation of the resulting function is relayed to the original function
// if the resulting ScopedFunctionCanceller is alive and the cancel() is not
// called.
// The inner Function that is passed to makeCancellable() will be destroyed
// when it's cancelled or the outer Function is destroyed.
//
// Example:
//   void foo() {}
//   std::unique_ptr<Function<void()>> function = WTF::bind(&foo);
//
//   auto result = makeCancellable(std::move(function));
//
//   (*result.function)();  // Not cancelled. foo() is called.
//   result.canceller.cancel();
//   (*result.function)();  // Cancelled. foo() is not called.
//
template <typename... Params>
MakeCancellableResult<Params...> makeCancellable(
    std::unique_ptr<Function<void(Params...)>> function) {
  using Canceller = internal::FunctionCancellerImpl<Params...>;
  RefPtr<Canceller> canceller = adoptRef(new Canceller(std::move(function)));

  // Implementation note:
  // Keep a ScopedFunctionCanceller instance in |wrappedFunction| below, so
  // that the destruction of |wrappedFunction| implies the destruction of
  // |function|. This is needed to avoid a circular strong reference among a
  // bound parameter, Function, and FunctionCanceller.
  //
  // E.g.:
  //   struct Foo : GarbageCollectedFinalized<Foo> {
  //       RefPtr<FunctionCanceller> m_canceller;
  //       void bar();
  //   };
  //
  //   Foo* foo = new Foo;
  //   auto result = makeCancellable(bind(&Foo::bar, wrapPersistent(foo)));
  //
  //   // Destruction of the resulting Function implies the destruction of
  //   // the original function via ScopedFunctionCanceller below, that
  //   // resolves a circular strong reference:
  //   //   foo -> m_canceller -> m_function -> foo
  //   result.function = nullptr;
  auto wrappedFunction = bind(&Canceller::runUnlessCancelled, canceller,
                              ScopedFunctionCanceller(canceller));
  return MakeCancellableResult<Params...>(
      ScopedFunctionCanceller(canceller.release()), std::move(wrappedFunction));
}

}  // namespace WTF

#endif  // WTF_MakeCancellable_h
