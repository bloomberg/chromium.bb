// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NETWORK_HINTS_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NETWORK_HINTS_INTERFACE_H_

#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class NetworkHintsInterface {
 public:
  virtual void DnsPrefetchHost(const String&) const = 0;
  virtual void PreconnectHost(const KURL&,
                              const CrossOriginAttributeValue) const = 0;
};

class NetworkHintsInterfaceImpl : public NetworkHintsInterface {
 public:
  void DnsPrefetchHost(const String& host) const override;

  void PreconnectHost(
      const KURL& host,
      const CrossOriginAttributeValue cross_origin) const override;
};

}  // namespace blink

#endif
