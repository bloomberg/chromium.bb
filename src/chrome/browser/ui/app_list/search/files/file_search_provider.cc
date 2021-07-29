// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_search_provider.h"

#include <cctype>
#include <cmath>

#include "base/files/file_enumerator.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

constexpr char kFileSearchSchema[] = "file_search://";
constexpr int kMaxResults = 25;
constexpr int kSearchTimeoutMs = 100;

// Construct a case-insensitive fnmatch query from |query|. E.g. for abc123, the
// result would be *[aA][bB][cC]123*.
std::string CreateFnmatchQuery(const std::string& query) {
  std::vector<std::string> query_pieces = {"*"};
  size_t sequence_start = 0;
  for (size_t i = 0; i < query.size(); ++i) {
    if (isalpha(query[i])) {
      if (sequence_start != i) {
        query_pieces.push_back(
            query.substr(sequence_start, i - sequence_start));
      }
      std::string piece("[");
      piece.resize(4);
      piece[1] = tolower(query[i]);
      piece[2] = toupper(query[i]);
      piece[3] = ']';
      query_pieces.push_back(std::move(piece));
      sequence_start = i + 1;
    }
  }
  if (sequence_start != query.size()) {
    query_pieces.push_back(query.substr(sequence_start));
  }
  query_pieces.push_back("*");

  return base::StrCat(query_pieces);
}

// Returns a vector of matched filepaths and a bool indicating whether or not
// the path is a directory.
std::vector<FileSearchProvider::PathInfo> SearchFilesByPattern(
    const base::FilePath& root_path,
    const std::string& query,
    const base::TimeTicks& query_start_time) {
  base::FileEnumerator enumerator(
      root_path,
      /*recursive=*/true,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES,
      CreateFnmatchQuery(query), base::FileEnumerator::FolderSearchPolicy::ALL);

  const auto time_limit = base::TimeDelta::FromMilliseconds(kSearchTimeoutMs);
  bool timed_out = false;
  std::vector<FileSearchProvider::PathInfo> matched_paths;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    matched_paths.emplace_back(
        path, enumerator.GetInfo().IsDirectory(),
        base::Time::FromTimeT(enumerator.GetInfo().stat().st_atime));

    if (matched_paths.size() == kMaxResults) {
      break;
    } else if (base::TimeTicks::Now() - query_start_time > time_limit) {
      timed_out = true;
      break;
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.FileSearchProvider.TimedOut", timed_out);
  return matched_paths;
}

}  // namespace

FileSearchProvider::FileSearchProvider(Profile* profile)
    : profile_(profile),
      root_path_(file_manager::util::GetMyFilesFolderForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(!root_path_.empty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FileSearchProvider::~FileSearchProvider() = default;

ash::AppListSearchResultType FileSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kFileSearch;
}

void FileSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();

  // Clear results and cancel any outgoing requests.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  // This provider does not handle zero-state.
  if (query.empty())
    return;

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(SearchFilesByPattern, root_path_, base::UTF16ToUTF8(query),
                     query_start_time_),
      base::BindOnce(&FileSearchProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr()));
}

void FileSearchProvider::OnSearchComplete(
    std::vector<FileSearchProvider::PathInfo> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Sort paths by the time of last access.
  std::sort(paths.begin(), paths.end(),
            [](const FileSearchProvider::PathInfo& a,
               const FileSearchProvider::PathInfo& b) {
              return a.last_accessed < b.last_accessed;
            });

  constexpr float kScoreEps = 1.0e-5f;
  SearchProvider::Results results;
  for (int i = 0; i < paths.size(); ++i) {
    // Add increasing score boosts for more recently accessed files.
    double relevance =
        FileResult::CalculateRelevance(last_tokenized_query_, paths[i].path);
    relevance += i * kScoreEps;
    results.emplace_back(MakeResult(paths[i], relevance));
  }

  SwapResults(&results);
  UMA_HISTOGRAM_TIMES("Apps.AppList.FileSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> FileSearchProvider::MakeResult(
    const FileSearchProvider::PathInfo& path,
    const double relevance) {
  const auto type = path.is_directory ? FileResult::Type::kDirectory
                                      : FileResult::Type::kFile;
  return std::make_unique<FileResult>(kFileSearchSchema, path.path,
                                      ash::AppListSearchResultType::kFileSearch,
                                      last_query_, relevance, type, profile_);
}

}  // namespace app_list
