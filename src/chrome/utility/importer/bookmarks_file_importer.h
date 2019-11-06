// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
#define CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/utility/importer/importer.h"

// Importer for bookmarks files.
class BookmarksFileImporter : public Importer {
 public:
  BookmarksFileImporter();

  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   ImporterBridge* bridge) override;

 private:
  ~BookmarksFileImporter() override;

  DISALLOW_COPY_AND_ASSIGN(BookmarksFileImporter);
};

#endif  // CHROME_UTILITY_IMPORTER_BOOKMARKS_FILE_IMPORTER_H_
