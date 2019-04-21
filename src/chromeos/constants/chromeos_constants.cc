// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_constants.h"

#define FPL FILE_PATH_LITERAL

namespace chromeos {

const base::FilePath::CharType kDriveCacheDirname[] = FPL("GCache");
const base::FilePath::CharType kNssCertDbPath[] = FPL(".pki/nssdb/cert9.db");
const base::FilePath::CharType kNssDirPath[] = FPL(".pki");
const base::FilePath::CharType kNssKeyDbPath[] = FPL(".pki/nssdb/key4.db");

}  // namespace chromeos
