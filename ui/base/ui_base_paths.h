// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_PATHS_H_
#define UI_BASE_UI_BASE_PATHS_H_
#pragma once

// This file declares path keys for the app module.  These can be used with
// the PathService to access various special directories and files.

namespace ui {

enum {
  PATH_START = 3000,

  DIR_LOCALES,              // Directory where locale resources are stored.

  FILE_RESOURCES_PAK,       // Path to the data .pak file which holds binary
                            // resources.

  // Valid only in development environment; TODO(darin): move these
  DIR_TEST_DATA,            // Directory where unit test data resides.

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace ui

#endif  // UI_BASE_UI_BASE_PATHS_H_
