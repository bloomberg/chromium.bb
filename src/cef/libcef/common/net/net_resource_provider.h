// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_RESOURCE_PROVIDER_H_
#define CEF_LIBCEF_COMMON_NET_RESOURCE_PROVIDER_H_
#pragma once

#include "base/memory/ref_counted_memory.h"

// This is called indirectly by the network layer to access resources.
scoped_refptr<base::RefCountedMemory> NetResourceProvider(int key);

#endif  // CEF_LIBCEF_COMMON_NET_RESOURCE_PROVIDER_H_
