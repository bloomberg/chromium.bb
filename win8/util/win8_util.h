// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_UTIL_WIN8_UTIL_H_
#define WIN8_UTIL_WIN8_UTIL_H_

namespace win8 {

// Returns true if this process is running in fullscreen Metro mode on Win8+.
// Callers should always prefer this method to base::win::IsMetroProcess() for
// UI properties dependent on the single window mode as this method is also
// aware of the Ash environment in which multiple Chrome windows can live.
bool IsSingleWindowMetroMode();

}  // namespace win8

#endif  // WIN8_UTIL_WIN8_UTIL_H_
