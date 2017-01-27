// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"

// This WebPresentationConnectionCallbacks.cpp, which includes only
// WebPresentationConnectionCallbacks.h, should be in Source/platform/exported,
// because WebPresentationConnectionCallbacks is not compiled without this cpp.
// So if we don't have this cpp, we will see unresolved symbol error
// when constructor/destructor's address is required.
