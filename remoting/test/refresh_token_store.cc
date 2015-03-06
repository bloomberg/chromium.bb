// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/refresh_token_store.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace {
const base::FilePath::CharType kTokenFileName[] =
    FILE_PATH_LITERAL("refresh_token.txt");
const base::FilePath::CharType kRemotingFolder[] =
    FILE_PATH_LITERAL("remoting");
const base::FilePath::CharType kRefreshTokenStoreFolder[] =
    FILE_PATH_LITERAL("refresh_token_store");

// Returns the FilePath of the token store file for |user_name|.
base::FilePath GetRefreshTokenDirPath(const std::string& user_name) {
  base::FilePath refresh_token_dir_path;
  if (!GetTempDir(&refresh_token_dir_path)) {
    LOG(WARNING) << "Failed to retrieve temporary directory path.";
    return base::FilePath();
  }

  refresh_token_dir_path = refresh_token_dir_path.Append(kRemotingFolder);
  refresh_token_dir_path = refresh_token_dir_path.Append(
      kRefreshTokenStoreFolder);

  // We call AppendASCII here our user_name is a std::string but wide strings
  // are used on WIN platforms.  ApendASCII will convert our std::string into
  // the correct type for windows platforms.
  refresh_token_dir_path = refresh_token_dir_path.AppendASCII(user_name);

  return refresh_token_dir_path;
}

}  // namespace

namespace remoting {
namespace test {

// Provides functionality to write a refresh token to a local folder on disk and
// read it back during subsequent tool runs.
class RefreshTokenStoreOnDisk : public RefreshTokenStore {
 public:
  explicit RefreshTokenStoreOnDisk(const std::string user_name);
  ~RefreshTokenStoreOnDisk() override;

  // RefreshTokenStore interface.
  std::string FetchRefreshToken() override;
  bool StoreRefreshToken(const std::string& refresh_token) override;

 private:
  // Used to access the user specific token file.
  std::string user_name_;

  DISALLOW_COPY_AND_ASSIGN(RefreshTokenStoreOnDisk);
};

RefreshTokenStoreOnDisk::RefreshTokenStoreOnDisk(const std::string user_name) :
    user_name_(user_name) {}

RefreshTokenStoreOnDisk::~RefreshTokenStoreOnDisk() {}

std::string RefreshTokenStoreOnDisk::FetchRefreshToken() {
  base::FilePath token_dir_path(GetRefreshTokenDirPath(user_name_));
  DCHECK(!token_dir_path.empty());

  DVLOG(2) << "Reading token from path: " << token_dir_path.value();
  base::FilePath token_file_path(token_dir_path.Append(kTokenFileName));

  std::string refresh_token;
  if (!base::ReadFileToString(token_file_path, &refresh_token)) {
    DVLOG(1) << "Failed to read token file from: " << token_dir_path.value();
    return std::string();
  }

  return refresh_token;
}

bool RefreshTokenStoreOnDisk::StoreRefreshToken(
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());

  base::FilePath token_dir_path(GetRefreshTokenDirPath(user_name_));
  if (token_dir_path.empty()) {
    return false;
  }

  base::FilePath token_file_path(token_dir_path.Append(kTokenFileName));
  if (!base::DirectoryExists(token_dir_path) &&
      !base::CreateDirectory(token_dir_path)) {
    LOG(ERROR) << "Failed to create directory, refresh token not stored.";
    return false;
  }

#if defined(OS_POSIX)
  // For POSIX we can set permissions on the token file so we do so here.
  // The test code should not run on other platforms since the code to safely
  // store the token has not been implemented yet.

  // Create an empty stub file if one does not exist.
  if (!base::PathExists(token_file_path) &&
      base::WriteFile(token_file_path, "", 0) < 0) {
    LOG(ERROR) << "Failed to create stub file, refresh token not stored.";
    return false;
  }

  // Set permissions on the stub file.
  int mode =
      base::FILE_PERMISSION_READ_BY_USER | base::FILE_PERMISSION_WRITE_BY_USER;
  if (!SetPosixFilePermissions(token_file_path, mode)) {
    LOG(ERROR) << "Failed to set file permissions, refresh token not stored.";
    return false;
  }

  // Write the refresh token to our newly created file.
  if (base::WriteFile(token_file_path, refresh_token.c_str(),
                      refresh_token.size()) < 0) {
    LOG(ERROR) << "Failed to save refresh token to the file on disk.";
    return false;
  }

  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // OS_POSIX
}

scoped_ptr<RefreshTokenStore> RefreshTokenStore::OnDisk(
    const std::string& user_name) {
  return make_scoped_ptr<RefreshTokenStore>(new RefreshTokenStoreOnDisk(
      user_name));
}

}  // namespace test
}  // namespace remoting
