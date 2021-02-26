// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_file_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/filename_util.h"
#include "ui/base/dragdrop/file_info/file_info.h"
#include "url/gurl.h"

namespace exo {

TestFileHelper::TestFileHelper() = default;

TestFileHelper::~TestFileHelper() = default;

std::vector<ui::FileInfo> TestFileHelper::GetFilenames(
    aura::Window* source,
    const std::vector<uint8_t>& data) const {
  std::string lines(data.begin(), data.end());
  std::vector<ui::FileInfo> filenames;
  for (const base::StringPiece& line : base::SplitStringPiece(
           lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path;
    if (net::FileURLToFilePath(GURL(line), &path))
      filenames.emplace_back(ui::FileInfo(path, base::FilePath()));
  }
  return filenames;
}

std::string TestFileHelper::GetMimeTypeForUriList(aura::Window* target) const {
  return "text/uri-list";
}

void TestFileHelper::SendFileInfo(aura::Window* target,
                                  const std::vector<ui::FileInfo>& files,
                                  SendDataCallback callback) const {
  std::vector<std::string> lines;
  for (const auto& file : files) {
    lines.emplace_back("file://" + file.path.value());
  }
  std::string result = base::JoinString(lines, "\r\n");
  std::move(callback).Run(base::RefCountedString::TakeString(&result));
}

bool TestFileHelper::HasUrlsInPickle(const base::Pickle& pickle) const {
  return true;
}

void TestFileHelper::SendPickle(aura::Window* target,
                                const base::Pickle& pickle,
                                SendDataCallback callback) {
  send_pickle_callback_ = std::move(callback);
}

void TestFileHelper::RunSendPickleCallback(std::vector<GURL> urls) {
  std::vector<std::string> lines;
  for (const auto& url : urls) {
    lines.emplace_back(url.spec());
  }
  std::string result = base::JoinString(lines, "\r\n");
  std::move(send_pickle_callback_)
      .Run(base::RefCountedString::TakeString(&result));
}

}  // namespace exo
