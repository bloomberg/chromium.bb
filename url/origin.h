// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ORIGIN_H_
#define URL_ORIGIN_H_

#include "url/deprecated_serialized_origin.h"

// TODO(mkwst): Drop this once Blink's WebSerializedOrigin no longer depends
// on this class name.
namespace url {
using Origin = DeprecatedSerializedOrigin;
}

#endif  // URL_ORIGIN_H_
