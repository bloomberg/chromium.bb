// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_PROTOCOL_SYNC_PROTO_EXPORT_H_
#define SYNC_PROTOCOL_SYNC_PROTO_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SYNC_PROTO_IMPLEMENTATION)
#define SYNC_PROTO_EXPORT __declspec(dllexport)
#else
#define SYNC_PROTO_EXPORT __declspec(dllimport)
#endif  // defined(SYNC_PROTO_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SYNC_PROTO_IMPLEMENTATION)
#define SYNC_PROTO_EXPORT __attribute__((visibility("default")))
#else
#define SYNC_PROTO_EXPORT
#endif  // defined(SYNC_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define SYNC_PROTO_EXPORT
#endif

#endif  // SYNC_PROTOCOL_SYNC_PROTO_EXPORT_H_
