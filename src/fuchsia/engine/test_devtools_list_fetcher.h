// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_DEVTOOLS_LIST_FETCHER_H_
#define FUCHSIA_ENGINE_TEST_DEVTOOLS_LIST_FETCHER_H_

#include "base/values.h"

// Returns the JSON value of the list URL for the DevTools service listening
// on port |port| on localhost. Returns an empty value on error.
base::Value GetDevToolsListFromPort(uint16_t port);

#endif  // FUCHSIA_ENGINE_TEST_DEVTOOLS_LIST_FETCHER_H_
