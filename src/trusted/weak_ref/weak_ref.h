/* -*- c++ -*- */
/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A form of weak reference for use by CallOnMainThread (COMT) and
// other callback situations.  This class is essentially a thread-safe
// refcounted class that is used to hold pointers to resources that
// may "go away" due to the plugin instance being destroyed.
// Generally resource cleanup should be handled by the contained
// class's dtor.
//
// Here is the intended use case.  Every plug-in instance contains a
// reference to a WeakRefAnchor object.  Callbacks must use only
// resources pointed to by a WeakRef object -- when COMT is invoked,
// the COMT-queue owns a reference to the WeakRef object.  When a
// WeakRefAnchor object is abandoned (typically in the plug-in dtor),
// it ensures that all associated callbacks will either not be called
// (if the callback takes a resource pointer) or will be invoked with
// a WeakRef<Resource>* object which yields NULL when the
// ReleaseAndUnref method is invoked on it.  After the callback fires,
// the resource associated with the callback is deleted.  This is
// required when, for example, the completion callback resource is the
// I/O buffer into which a Read operation is writing; we must wait for
// the Read completion event before we can deallocate the destination
// buffer.
//
// In the normal execution, the callback data is present, and the
// callback function should or save its contents elsewhere if needed;
// the callback data is deleted after the callback function returns.


// EXAMPLE USAGE
//
// class Foo {
//   Foo() : anchor_(new WeakRefAnchor);
//   ~Foo() {
//     anchor_->Abandon();
//   }
//   static void DoOperation_MainThread(void* vargs, int32_t result) {
//     WeakRef<OpArgument>* args =
//       reinterpret_cast<WeakRef<OpArgument>*>(vargs);
//     scoped_ptr<OpArgument> p;
//     args->ReleaseAndUnref(&p);
//     if (p != NULL) {
//       ... stash away elements of p...
//     }
//   }
//   ScheduleOperation() {
//      OpArgument* args = new OpArgument(...);
//      pp::CompletionCallback cc(DoOperation_MainThread,
//   WeakRefAnchor* anchor_;
// };
//
// Improved syntactic sugar makes this even simpler in
// <call_on_main_thread.h>.


// This class operates much like CompletionCallbackFactory (if
// instantiated with a thread-safe RefCount class), except that
// appropriate memory barriers are included to ensure that partial
// memory writes are not seen by other threads.  Here is the memory
// consistency issue:
//
// In <completion_callback.h>'s CompletionCallbackFactory<T>, the only
// memory barriers are with the refcount adjustment.  In
// ResetBackPointer, we follow DropFactory() immediately by Release().
// Imagine another thread that reads the factory_ value via
// back_pointer_->GetObject().  The assignment of NULL to
// back_pointer_->factory_ may result in more than one bus cycle. That
// means a read from another thread may yield a garbled value,
// comprised of some combination of the original pointer bits and
// zeros -- this may occur even on Intel architectures where any
// visible memory changes must be in the original write order, if the
// pointer were to span cache lines (e.g., packed attribute used).

#ifndef NATIVE_CLIENT_SRC_TRUSTED_WEAK_REF_WEAK_REF_H_
#define NATIVE_CLIENT_SRC_TRUSTED_WEAK_REF_WEAK_REF_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/refcount_base.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"

namespace nacl {

static char const* const kWeakRefModuleName = "weak_ref";

class WeakRefAnchor;

class AnchoredResource : public RefCountBase {
 public:
  explicit AnchoredResource(WeakRefAnchor* anchor);

  virtual ~AnchoredResource();

  // covariant impl of Ref()
  AnchoredResource* Ref() {
    return reinterpret_cast<AnchoredResource*>(RefCountBase::Ref());
  }

 protected:
  friend class WeakRefAnchor;

  WeakRefAnchor* anchor_;
  NaClMutex mu_;

  DISALLOW_COPY_AND_ASSIGN(AnchoredResource);
};

template <typename R> class WeakRef;  // fwd

// Plugin instance should hold a reference, and pass new references to
// the WeakRefAnchor to any threads that may need to create callbacks.
// The plugin instance should invoke Abandon before Unref'ing its
// pointer to its WeakRefAnchor object.  When all threads that may
// want to invoke COMT have exited (through some unspecified
// notification mechanism, e.g., RPC channel closure, then the
// WeakRefAnchor will get reclaimed.

class WeakRefAnchor : public RefCountBase {
 public:
  WeakRefAnchor();

  // covariant impl of Ref()
  WeakRefAnchor* Ref() {
    return reinterpret_cast<WeakRefAnchor*>(RefCountBase::Ref());
  }
  bool is_abandoned();

  // Mark anchor as abandoned.  Does not Unref(), so caller still has
  // to do that.
  void Abandon();

  // If MakeWeakRef fails, i.e., the WeakRefAnchor had been abandoned
  // in the main thread, then it returns NULL and the caller is
  // responsible for releasing the resource represented by
  // raw_resource; if MakeWeakRef succeeds, then it takes ownership of
  // raw_resource and has wrapped in in the WeakRef<R>* object, which
  // can be used as argument to the callback function for COMT.  After
  // ownership passes, the caller must not continue to use the
  // raw_resource, since the main thread may Abandon the anchor at any
  // time and cause raw_resource to have its dtor invoked in
  // WeakRef<R>::reset_mu.
  template <typename R>
  WeakRef<R>* MakeWeakRef(R* raw_resource) {
    WeakRef<R>* rp = NULL;
    NaClLog2(kWeakRefModuleName, 4,
             "Entered WeakRef<R>::MakeWeakRef, raw 0x%" NACL_PRIxPTR "\n",
             (uintptr_t) raw_resource);
    CHECK(raw_resource != NULL);
    rp = new WeakRef<R>(this, raw_resource);
    // If the WeakRefAnchor was already abandoned, we make a WeakRef anyway,
    // and the use of the WeakRef will discover this.
    CHECK(rp != NULL);
    NaClLog2(kWeakRefModuleName, 4,
             "Leaving WeakRef<R>::MakeWeakRef, weak_ref 0x%" NACL_PRIxPTR "\n",
             (uintptr_t) rp);
    return rp;
  }

 protected:
  ~WeakRefAnchor();

 private:
  NaClMutex mu_;

  bool abandoned_;

  DISALLOW_COPY_AND_ASSIGN(WeakRefAnchor);
};

// The resource type R should contain everything that a callback
// function needs.  In particular, it may contain a reference to a
// WeakRefAnchor object if the callback function may in turn want to
// schedule more callbacks -- and the WeakRefAnchor refcount should be
// properly managed (releasing when R is destroyed, incremented when a
// reference is passed to another resource R' for the chained
// callback).  If a reference to the plugin or other associated object
// is needed, that can be a "naked" pointer in the callback use case
// since the callback is only invoked on the main thread, and the
// plugin can only be shut down in the main thread (it cannot go
// away).  In general, for use cases where the WeakRef resource might
// be used in an arbitrary other thread, no naked, non-refcounted
// objects should be referred to from WeakRef wrapped resources.
//
template <typename R>
class WeakRef : public AnchoredResource {
  // public base class, needed for win64_newlib_dbg.  why?
 public:
  // Single use!  Invoked only from main thread, so anchor will not be
  // doing Abandon() at the same time.  After ReleaseAndUnref, caller
  // must not use the pointer to the WeakRef object further.
  void ReleaseAndUnref(scoped_ptr<R>* out_ptr) {
    NaClLog2(kWeakRefModuleName, 4,
             "Entered WeakRef<R>::ReleaseAndUnref: this 0x%" NACL_PRIxPTR "\n",
             (uintptr_t) this);
    do {
      nacl::MutexLocker take(&mu_);
      if (anchor_->is_abandoned()) {
        delete resource_.release();
        out_ptr->reset();
      } else {
        out_ptr->reset(resource_.release());
      }
    } while (0);
    NaClLog2(kWeakRefModuleName, 4,
             "Leaving ReleaseAndUnref: raw: out_ptr->get() 0x%"
             NACL_PRIxPTR "\n",
             (uintptr_t) out_ptr->get());
    Unref();  // release ref associated with the callback
  }

 protected:
  virtual ~WeakRef() {}

 private:
  friend class WeakRefAnchor;

  WeakRef(WeakRefAnchor* anchor, R* resource)
      : AnchoredResource(anchor),
        resource_(resource) {
    CHECK(resource != NULL);
  }

  scoped_ptr<R> resource_;  // NULL when anchor object is destroyed.

  DISALLOW_COPY_AND_ASSIGN(WeakRef);
};

}  // namespace nacl

#endif
