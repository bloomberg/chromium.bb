// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_NAMES_H_
#define SERVICES_SHELL_PUBLIC_CPP_NAMES_H_

#include <string>

namespace shell {

extern const char kNameType_Service[];
extern const char kNameType_Exe[];

// Services are identified by structured "names", of the form:
//
//    type:path.
//
// The type field tells the shell how to load the service. Two types are
// recognized:
//
//  service
//   A synthetic service name, which may be packaged as part of another service
//   (via ServiceFactory) or as a standalone .library. .library files live
//   within a directory hierarchy alongside the service manager executable.
//
//  exe
//   Represents a native executable on the host platform, expected to live
//   alongside the service manager executable. Executables launched via this
//   mechanism are passed a handle to the service manager on the command line
//   and are expected to bind a ServiceRequest enabling further communication
//   with the Service Manager. The path component contains the executable name,
//   minus any platform specific extension.
//
// Other types may be supplied but are not recognized by any of the
// NativeRunners, and as such custom loaders must be specified for such names.
//
// Any name type may serve as an alias for any other name type. Aliasing is
// resolved implicitly by the Shell.

// Returns true if the name is a valid form, i.e. type:path. path cannot start
// with a "//" sequence. These are _not_ urls.
bool IsValidName(const std::string& name);

// Get the type component of the specified name.
std::string GetNameType(const std::string& name);

// Get the path component of the specified name.
std::string GetNamePath(const std::string& name);

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_NAMES_H_
