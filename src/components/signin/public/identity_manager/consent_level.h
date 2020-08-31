// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_CONSENT_LEVEL_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_CONSENT_LEVEL_H_

namespace signin {

// ConsentLevel is the required level of user consent for an identity operation
// (for example to fetch an OAuth2 access token).
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.identitymanager
enum class ConsentLevel {
  // No specific consent required. In particular, browser sync consent is not
  // required. Operations are allowed if the user is signed in to Chrome. See
  // "unconsented primary account" in ./README.md.
  kNotRequired,

  // Chrome browser sync consent is required. Historically (before DICE and
  // Project Butter) most operations implicitly required this consent. See
  // "primary account" in ./README.md.
  kSync
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_CONSENT_LEVEL_H_
