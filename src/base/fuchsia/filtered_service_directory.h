// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_FILTERED_SERVICE_DIRECTORY_H_
#define BASE_FUCHSIA_FILTERED_SERVICE_DIRECTORY_H_

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/zx/channel.h>
#include <memory>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace base {
namespace fuchsia {

// ServiceDirectory that uses the supplied sys::ServiceDirectory to satisfy
// requests for only a restricted set of services.
class BASE_EXPORT FilteredServiceDirectory {
 public:
  // Creates a directory that proxies requests to the specified service
  // |directory|.
  explicit FilteredServiceDirectory(sys::ServiceDirectory* directory);
  ~FilteredServiceDirectory();

  // Adds the specified service to the list of whitelisted services.
  void AddService(base::StringPiece service_name);

  // Connects a directory client. The directory can be passed to a sandboxed
  // process to be used for /svc namespace.
  void ConnectClient(
      fidl::InterfaceRequest<::fuchsia::io::Directory> dir_request);

  // Accessor for the OutgoingDirectory, used to add handlers for services
  // in addition to those provided from |directory| via AddService().
  sys::OutgoingDirectory* outgoing_directory() { return &outgoing_directory_; }

 private:
  const sys::ServiceDirectory* const directory_;
  sys::OutgoingDirectory outgoing_directory_;

  DISALLOW_COPY_AND_ASSIGN(FilteredServiceDirectory);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_FILTERED_SERVICE_DIRECTORY_H_
