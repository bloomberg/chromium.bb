// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTERNAL_INSTALL_INFO_H_
#define EXTENSIONS_BROWSER_EXTERNAL_INSTALL_INFO_H_

#include "base/files/file_path.h"
#include "base/version.h"
#include "extensions/common/manifest.h"
#include "url/gurl.h"

namespace extensions {

// Holds information about an external extension install from an external
// provider.
struct ExternalInstallInfo {
  ExternalInstallInfo(const std::string& extension_id,
                      int creation_flags,
                      bool mark_acknowledged);
  ExternalInstallInfo(ExternalInstallInfo&& other);
  virtual ~ExternalInstallInfo() {}

  std::string extension_id;
  int creation_flags;
  bool mark_acknowledged;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalInstallInfo);
};

struct ExternalInstallInfoFile : public ExternalInstallInfo {
  ExternalInstallInfoFile(const std::string& extension_id,
                          const base::Version& version,
                          const base::FilePath& path,
                          Manifest::Location crx_location,
                          int creation_flags,
                          bool mark_acknowledged,
                          bool install_immediately);
  ExternalInstallInfoFile(ExternalInstallInfoFile&& other);
  ~ExternalInstallInfoFile() override;

  base::Version version;
  base::FilePath path;
  Manifest::Location crx_location;
  bool install_immediately;
};

struct ExternalInstallInfoUpdateUrl : public ExternalInstallInfo {
  ExternalInstallInfoUpdateUrl(const std::string& extension_id,
                               const std::string& install_parameter,
                               GURL update_url,
                               Manifest::Location download_location,
                               int creation_flags,
                               bool mark_acknowledged);
  ExternalInstallInfoUpdateUrl(ExternalInstallInfoUpdateUrl&& other);
  ~ExternalInstallInfoUpdateUrl() override;

  std::string install_parameter;
  GURL update_url;
  Manifest::Location download_location;
};

}  // namespace extensions
#endif  // EXTENSIONS_BROWSER_EXTERNAL_INSTALL_INFO_H_
