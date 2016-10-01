// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataConsumerHandle.h"

namespace blink {

std::unique_ptr<WebDataConsumerHandle::Reader>
FetchDataConsumerHandle::obtainReader(Client* client) {
  return obtainFetchDataReader(client);
}

}  // namespace blink
