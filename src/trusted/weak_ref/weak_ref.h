/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A form of weak reference for use by CallOnMainThread (COMT).  This
// class is essentially a thread-safe refcounted class that is used to
// hold pointers to resources that may "go away" due to the plugin
// instance being destroyed.  Generally resource cleanup should be
// handled by the contained class's dtor.
//
// Here is the intended use case.  Every plug-in instance contains a
// reference to a WeakRefAnchor object.  Callbacks must use only
// resources pointed to by a WeakRef object -- when COMT is invoked,
// the COMT-queue owns a reference to the WeakRef object.  When a
// WeakRefAnchor object is abandoned (typically in the plug-in dtor),
// it ensures that all associated callbacks will get NULL resource
// pointers when they eventually run.  It does this by keeping track
// of all WeakRef objects in pending COMT callbacks and eagerly
// destroys all resources associated with the queued callbacks.  The
// WeakRef resource pointer is released, triggering the dtor for the
// resource, and the WeakRef reference is also decremented, so that
// when the COMT callback is invoked, it can discover that the
// associated callback data is gone, and will just abort by
// decrementing the last reference to the WeakRef object.
//
// In the normal execution, the callback data is present, and
// ownership is released to the callback function.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_WEAK_REF_WEAK_REF_H_
#define NATIVE_CLIENT_SRC_TRUSTED_WEAK_REF_WEAK_REF_H_

#include <set>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/refcount_base.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

namespace nacl {

char const *const kWeakRefModuleName = "weak_ref";

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

  void Abandon();

  virtual void reset_mu() = 0;

  WeakRefAnchor* anchor_;
  NaClMutex mu_;
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

  void Abandon();  // iterate through tracked_ and release all resources.

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
    CHECK(raw_resource != NULL);
    NaClLog2(kWeakRefModuleName, 2,
             "Entered WeakRef<R>::MakeWeakRef, raw 0x%"NACL_PRIxPTR"\n",
             (uintptr_t) raw_resource);
    NaClXMutexLock(&mu_);
    if (!abandoned_) {
      rp = new WeakRef<R>(this, raw_resource);
      tracked_.insert(rp->Ref());
    }
    NaClXMutexUnlock(&mu_);
    NaClLog2(kWeakRefModuleName, 3,
             "Leaving WeakRef<R>::MakeWeakRef, weak_ref 0x%"NACL_PRIxPTR"\n",
             (uintptr_t) rp);
    return rp;
  }

  void Remove(AnchoredResource *weak_ref) {
    NaClLog2(kWeakRefModuleName, 3,
             "WeakRefAnchor::Remove: weak_ref 0x%"NACL_PRIxPTR"\n",
             (uintptr_t) weak_ref);
    NaClXMutexLock(&mu_);
    tracked_.erase(weak_ref);
    weak_ref->Unref();
    NaClXMutexUnlock(&mu_);
  }

 protected:
  ~WeakRefAnchor() {
    NaClMutexDtor(&mu_);
  }  // only via Unref

 private:
  NaClMutex mu_;

  bool abandoned_;

  struct ltptr {
    bool operator()(const AnchoredResource* left,
                    const AnchoredResource* right) const {
      return (reinterpret_cast<uintptr_t>(left)
              < reinterpret_cast<uintptr_t>(right));
    }
  };

  typedef std::set<AnchoredResource*, ltptr> container_type;
  container_type tracked_;
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
    NaClLog2(kWeakRefModuleName, 3,
             "Entered WeakRef<R>::ReleaseAndUnref: this 0x%"NACL_PRIxPTR"\n",
             (uintptr_t) this);
    NaClXMutexLock(&mu_);
    if (anchor_ == NULL) {
      // resource long gone
      out_ptr->reset();
    } else {
      out_ptr->reset(resource_.release());
      anchor_->Remove(this);
    }
    NaClXMutexUnlock(&mu_);
    Unref();
    NaClLog2(kWeakRefModuleName, 2,
             "Leaving ReleaseAndUnref: raw: out_ptr->get() 0x%"NACL_PRIxPTR"\n",
             (uintptr_t) out_ptr->get());
  }

 protected:
  void reset_mu() {
    NaClLog2(kWeakRefModuleName, 2,
             ("weak_ref<R>::reset_mu: this 0x%"NACL_PRIxPTR
              ", raw 0x%"NACL_PRIxPTR"\n"),
             (uintptr_t) this,
             (uintptr_t) resource_.get());
    R* rp = resource_.release();
    CHECK(rp != NULL);
    delete rp;
  }

  virtual ~WeakRef() {
    CHECK(resource_.get() == NULL);
  }

 private:
  friend class WeakRefAnchor;

  WeakRef(WeakRefAnchor* anchor, R* resource)
      : AnchoredResource(anchor),
        resource_(resource) {
    CHECK(resource != NULL);
  }

  scoped_ptr<R> resource_;  // NULL when anchor object is destroyed.
};

}  // namespace nacl

#endif
