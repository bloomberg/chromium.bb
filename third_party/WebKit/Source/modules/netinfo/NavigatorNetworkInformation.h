// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorNetworkInformation_h
#define NavigatorNetworkInformation_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"

namespace blink {

class Navigator;
class NetworkInformation;

class NavigatorNetworkInformation final
    : public GarbageCollected<NavigatorNetworkInformation>,
      public Supplement<Navigator>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorNetworkInformation);

 public:
  static NavigatorNetworkInformation& From(Navigator&);
  static NavigatorNetworkInformation* ToNavigatorNetworkInformation(Navigator&);
  static NetworkInformation* connection(Navigator&);

  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorNetworkInformation(Navigator&);
  NetworkInformation* connection();

  static const char* SupplementName();

  Member<NetworkInformation> connection_;
};

}  // namespace blink

#endif  // NavigatorNetworkInformation_h
