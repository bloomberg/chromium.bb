// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_WEB_ENGINE_EXPORT_H_
#define FUCHSIA_ENGINE_WEB_ENGINE_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WEB_ENGINE_IMPLEMENTATION)
#define WEB_ENGINE_EXPORT __attribute__((visibility("default")))
#else
#define WEB_ENGINE_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define WEB_ENGINE_EXPORT
#endif

#endif  // FUCHSIA_ENGINE_WEB_ENGINE_EXPORT_H_
