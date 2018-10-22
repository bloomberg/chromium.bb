// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_CONFIG_H_
#define ASH_PUBLIC_CPP_APP_CONFIG_H_

namespace ash {

// Enumeration of the possible configurations supported by ash.
// TODO(sky): this should go away entirely.
enum class Config {
  // Classic mode does not use mus.
  CLASSIC,

  // Aura is backed by mus and chrome and ash are in separate processes. In this
  // mode chrome code can only use ash code in ash/public/cpp.
  MASH_DEPRECATED,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_CONFIG_H_
