// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_PUBLIC_CPP_MANIFEST_H_
#define COMPONENTS_SERVICES_QUARANTINE_PUBLIC_CPP_MANIFEST_H_

namespace service_manager {
struct Manifest;
}

namespace quarantine {

const service_manager::Manifest& GetQuarantineManifest();

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_PUBLIC_CPP_MANIFEST_H_
