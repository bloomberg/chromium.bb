// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSecurityStyle_h
#define WebSecurityStyle_h
namespace blink {
// This enum represents the security state of a resource.
enum WebSecurityStyle {
  kWebSecurityStyleUnknown,
  kWebSecurityStyleNeutral,
  kWebSecurityStyleInsecure,
  kWebSecurityStyleSecure,
  kWebSecurityStyleLast = kWebSecurityStyleSecure
};
}  // namespace blink
#endif
