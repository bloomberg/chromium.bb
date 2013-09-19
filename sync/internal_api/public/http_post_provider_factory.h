// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_HTTP_POST_PROVIDER_FACTORY_H_
#define SYNC_INTERNAL_API_PUBLIC_HTTP_POST_PROVIDER_FACTORY_H_

#include <string>

#include "sync/base/sync_export.h"

namespace syncer {

class HttpPostProviderInterface;

// A factory to create HttpPostProviders to hide details about the
// implementations and dependencies.
// A factory instance itself should be owned by whomever uses it to create
// HttpPostProviders.
class SYNC_EXPORT HttpPostProviderFactory {
 public:
  virtual ~HttpPostProviderFactory() {}

  virtual void Init(const std::string& user_agent) = 0;

  // Obtain a new HttpPostProviderInterface instance, owned by caller.
  virtual HttpPostProviderInterface* Create() = 0;

  // When the interface is no longer needed (ready to be cleaned up), clients
  // must call Destroy().
  // This allows actual HttpPostProvider subclass implementations to be
  // reference counted, which is useful if a particular implementation uses
  // multiple threads to serve network requests.
  virtual void Destroy(HttpPostProviderInterface* http) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_HTTP_POST_PROVIDER_FACTORY_H_
