// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkHintsInterface_h
#define NetworkHintsInterface_h

#include "platform/network/NetworkHints.h"

namespace blink {

class NetworkHintsInterface {
 public:
  virtual void DnsPrefetchHost(const String&) const = 0;
  virtual void PreconnectHost(const KURL&,
                              const CrossOriginAttributeValue) const = 0;
};

class NetworkHintsInterfaceImpl : public NetworkHintsInterface {
  void DnsPrefetchHost(const String& host) const override { PrefetchDNS(host); }

  void PreconnectHost(
      const KURL& host,
      const CrossOriginAttributeValue cross_origin) const override {
    Preconnect(host, cross_origin);
  }
};

}  // namespace blink

#endif
