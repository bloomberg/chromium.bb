// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CRX_FILE_INFO_H_
#define EXTENSIONS_BROWSER_CRX_FILE_INFO_H_

#include <string>

#include "base/files/file_path.h"

namespace crx_file {
enum class VerifierFormat;
}

namespace extensions {

// CRXFileInfo holds general information about a cached CRX file
struct CRXFileInfo {
  CRXFileInfo();
  CRXFileInfo(const std::string& extension_id,
              const base::FilePath& path,
              const std::string& hash,
              const crx_file::VerifierFormat required_format);
  CRXFileInfo(const std::string& extension_id,
              const crx_file::VerifierFormat required_format,
              const base::FilePath& path);
  CRXFileInfo(const base::FilePath& path,
              const crx_file::VerifierFormat required_format);
  explicit CRXFileInfo(const CRXFileInfo&);

  bool operator==(const CRXFileInfo& that) const;

  // Only |path| and |required_format| are mandatory. |extension_id| and
  // |expected_hash| are only checked if non-empty.
  std::string extension_id;
  base::FilePath path;
  std::string expected_hash;
  crx_file::VerifierFormat required_format;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CRX_FILE_INFO_H_
