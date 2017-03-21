// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/switches.h"

namespace switches {

// Describes the file descriptors passed to a child process in the following
// list format:
//
//     <file_id>:<descriptor_id>,<file_id>:<descriptor_id>,...
//
// where <file_id> is an ID string from the manifest of the service being
// launched and <descriptor_id> is the numeric identifier of the descriptor for
// the child process can use to retrieve the file descriptor from the
// global descriptor table.
const char kSharedFiles[] = "shared-files";

}  // namespace switches
