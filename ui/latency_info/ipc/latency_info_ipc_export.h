// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_INFO_LATENCY_INFO_IPC_EXPORT_H_
#define UI_LATENCY_INFO_LATENCY_INFO_IPC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(LATENCY_INFO_IPC_IMPLEMENTATION)
#define LATENCY_INFO_IPC_EXPORT __declspec(dllexport)
#else
#define LATENCY_INFO_IPC_EXPORT __declspec(dllimport)
#endif  // defined(LATENCY_INFO_IPC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(LATENCY_INFO_IPC_IMPLEMENTATION)
#define LATENCY_INFO_IPC_EXPORT __attribute__((visibility("default")))
#else
#define LATENCY_INFO_IPC_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define LATENCY_INFO_IPC_EXPORT
#endif

#endif  // UI_LATENCY_INFO_LATENCY_INFO_IPC_EXPORT_H_
