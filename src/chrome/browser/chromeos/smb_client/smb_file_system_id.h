// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_FILE_SYSTEM_ID_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_FILE_SYSTEM_ID_H_

#include <string>

#include "base/files/file.h"

namespace chromeos {
namespace smb_client {

// Creates a FileSystemId by concatenating a random filesystem identifier and
// |share_path| with a delimiter. The random ID is used so that the same share
// path can be mounted multiple times. If |is_kerberos_chromad| is set, an
// additional symbol is appended.
std::string CreateFileSystemId(const base::FilePath& share_path,
                               bool is_kerberos_chromad);

// Returns the SharePath component of a |file_system_id|. |file_system_id| must
// be well-formed (e.g. 2@@smb://192.168.1.1/testShare).
base::FilePath GetSharePathFromFileSystemId(const std::string& file_system_id);

// Returns whether |file_system_id| corresponds to a share that was mounted
// using ChromAD Kerberos.
bool IsKerberosChromadFileSystemId(const std::string& file_system_id);

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_FILE_SYSTEM_ID_H_
