// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_NAMES_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_NAMES_H_

#include <string>

namespace service_manager {

// TODO(beng): Eliminate this file entirely and drop the service: prefix.

// Services are identified by structured "names", of the form:
//
//    service:<name>
//
// This name is an alias that helps the Service Manager locate an implementation
// of the Service interface. The alias may be to some object constructed and
// registered with the Service Manager, or be provided by a Service library file
// extant on disk in the location EXE_DIR/Packages/<name>/<name>.library.

// Returns true if the name is a valid form, i.e. service:<name>. path cannot
// start with a "//" sequence. These are _not_ urls.
bool IsValidName(const std::string& name);

// Get the path component of the specified name.
std::string GetNamePath(const std::string& name);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_NAMES_H_
