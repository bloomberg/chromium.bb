// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace cloud_print {

// static
std::unique_ptr<PrivetHTTPAsynchronousFactory>
PrivetHTTPAsynchronousFactory::CreateInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return base::WrapUnique<PrivetHTTPAsynchronousFactory>(
      new PrivetHTTPAsynchronousFactoryImpl(url_loader_factory));
}

}  // namespace cloud_print
