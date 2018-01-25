// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterfaceInvalidator_h
#define InterfaceInvalidator_h

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "platform/PlatformExport.h"

namespace blink {

// Notifies weak interface bindings to be invalidated when this object is
// destroyed.
class PLATFORM_EXPORT InterfaceInvalidator {
 public:
  InterfaceInvalidator();
  ~InterfaceInvalidator();

  class Observer {
   public:
    virtual void OnInvalidate() = 0;
  };

  void AddObserver(Observer*);
  void RemoveObserver(const Observer*);

  base::WeakPtr<InterfaceInvalidator> GetWeakPtr();

 private:
  void NotifyInvalidate();

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<InterfaceInvalidator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceInvalidator);
};

}  // namespace blink

#endif  // InterfaceInvalidator_h
