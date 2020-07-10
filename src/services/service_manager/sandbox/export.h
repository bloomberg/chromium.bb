// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_EXPORT_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SERVICE_MANAGER_SANDBOX_IMPL)
#define SERVICE_MANAGER_SANDBOX_EXPORT __declspec(dllexport)
#else
#define SERVICE_MANAGER_SANDBOX_EXPORT __declspec(dllimport)
#endif  // defined(SERVICE_MANAGER_SANDBOX_IMPL)

#else  // defined(WIN32)
#if defined(SERVICE_MANAGER_SANDBOX_IMPL)
#define SERVICE_MANAGER_SANDBOX_EXPORT __attribute__((visibility("default")))
#else
#define SERVICE_MANAGER_SANDBOX_EXPORT
#endif  // defined(SERVICE_MANAGER_SANDBOX_IMPL)
#endif

#else  // defined(COMPONENT_BUILD)
#define SERVICE_MANAGER_SANDBOX_EXPORT
#endif

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_EXPORT_H_
