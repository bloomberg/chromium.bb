// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/testing/WorkerInternalsFetch.h"

#include "modules/fetch/Response.h"
#include "wtf/Vector.h"

namespace blink {

Vector<String> WorkerInternalsFetch::getInternalResponseURLList(
    WorkerInternals& internals,
    Response* response) {
  if (!response)
    return Vector<String>();
  Vector<String> urlList;
  urlList.reserveCapacity(response->internalURLList().size());
  for (const auto& url : response->internalURLList())
    urlList.push_back(url);
  return urlList;
}

}  // namespace blink
