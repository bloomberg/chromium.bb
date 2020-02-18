// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/timer/timer.h"

namespace service_manager {

class ServiceBinding;
class ServiceKeepaliveRef;

// Service implementations are responsible for managing their own lifetime and
// as such are expected to call |ServiceBinding::RequestClose()| on their own
// ServiceBinding when they are no longer in use by any clients and otherwise
// have no reason to keep running (e.g. no active UI visible).
//
// ServiceKeepalive helps Service implementations accomplish this by vending
// instances of ServiceKeepaliveRef (see |CreateRef()| below). Once any
// ServiceKeepaliveRef has been created by a ServiceKeepalive, the
// ServiceKeepalive begins keeping track of the number of existing
// ServiceKeepaliveRefs.
//
// If the ServiceKeepalive's number of living ServiceKeepaliveRef instances goes
// to zero, the service is considered idle. If the ServiceKeepalive is
// configured with an idle timeout, it will automatically invoke
// |ServiceBinding::RequestClose()| on its associated ServiceBinding once the
// service has remained idle for that continuous duration.
//
// Services can use this mechanism to vend ServiceKeepaliveRefs to various parts
// of their implementation (e.g. to individual bound interface implementations)
// in order to safely and cleanly distribute their lifetime control.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ServiceKeepalive {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    // Invoked whenever the ServiceKeepalive detects that the service has been
    // idle for at least the idle time delta specified (if any) upon
    // construction of the ServiceKeepalive.
    virtual void OnIdleTimeout() {}

    // Invoked whenever the ServiceKeepalive detects new activity again after
    // having been idle for any amount of time.
    virtual void OnIdleTimeoutCancelled() {}
  };

  // Constructs a ServiceKeepalive to control the lifetime behavior of
  // |*binding|. Note that if either |binding| or |idle_timeout| is null, this
  // object will not do any automatic lifetime management and will instead only
  // maintain an internal ref-count which the consumer can query.
  ServiceKeepalive(ServiceBinding* binding,
                   base::Optional<base::TimeDelta> idle_timeout);
  ~ServiceKeepalive();

  // Constructs a new ServiceKeepaliveRef associated with this ServiceKeepalive.
  // New refs may be created either by calling this method again or by calling
  // |Clone()| on any another ServiceKeepaliveRef.
  std::unique_ptr<ServiceKeepaliveRef> CreateRef();

  // Returns |true| iff there are no existing ServiceKeepaliveRef instances
  // associated with this ServiceKeepalive.
  bool HasNoRefs();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class ServiceKeepaliveRefImpl;

  void AddRef();
  void ReleaseRef();

  void OnTimerExpired();

  ServiceBinding* const binding_;
  const base::Optional<base::TimeDelta> idle_timeout_;
  base::Optional<base::OneShotTimer> idle_timer_;
  base::ObserverList<Observer> observers_;
  int ref_count_ = 0;
  base::WeakPtrFactory<ServiceKeepalive> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceKeepalive);
};

// Objects which can be created by a |ServiceKeepalive| and cloned from each
// other. The ServiceBinding referenced by a ServiceKeepalive is considered
// active as long as one of these objects exists and is associated with that
// ServiceKeepalive.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ServiceKeepaliveRef {
 public:
  virtual ~ServiceKeepaliveRef() {}

  // Creates a new ServiceKeepaliveRef associated with the same ServiceKeepalive
  // as |this|.
  virtual std::unique_ptr<ServiceKeepaliveRef> Clone() = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_
