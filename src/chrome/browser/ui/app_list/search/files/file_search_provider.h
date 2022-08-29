// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SEARCH_PROVIDER_H_

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace app_list {

class FileResult;

class FileSearchProvider : public SearchProvider {
 public:
  struct PathInfo {
    base::FilePath path;
    bool is_directory;
    base::Time last_accessed;

    PathInfo(const base::FilePath& path,
             const bool is_directory,
             const base::Time& last_accessed)
        : path(path),
          is_directory(is_directory),
          last_accessed(last_accessed) {}
  };

  explicit FileSearchProvider(Profile* profile);
  ~FileSearchProvider() override;

  FileSearchProvider(const FileSearchProvider&) = delete;
  FileSearchProvider& operator=(const FileSearchProvider&) = delete;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() const override;
  void Start(const std::u16string& query) override;

  void SetRootPathForTesting(const base::FilePath& root_path) {
    root_path_ = root_path;
  }

 private:
  void OnSearchComplete(std::vector<FileSearchProvider::PathInfo> paths);
  std::unique_ptr<FileResult> MakeResult(
      const FileSearchProvider::PathInfo& path,
      const double relevance);

  base::TimeTicks query_start_time_;
  std::u16string last_query_;
  absl::optional<chromeos::string_matching::TokenizedString>
      last_tokenized_query_;

  Profile* const profile_;
  ash::ThumbnailLoader thumbnail_loader_;
  base::FilePath root_path_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FileSearchProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SEARCH_PROVIDER_H_
