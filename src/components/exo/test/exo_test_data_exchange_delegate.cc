// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_data_exchange_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace exo {

TestDataExchangeDelegate::TestDataExchangeDelegate() = default;

TestDataExchangeDelegate::~TestDataExchangeDelegate() = default;

ui::EndpointType TestDataExchangeDelegate::GetDataTransferEndpointType(
    aura::Window* window) const {
  return endpoint_type_;
}

std::vector<ui::FileInfo> TestDataExchangeDelegate::GetFilenames(
    ui::EndpointType source,
    const std::vector<uint8_t>& data) const {
  std::string lines(data.begin(), data.end());
  std::vector<ui::FileInfo> filenames;
  for (const base::StringPiece& line : base::SplitStringPiece(
           lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path;
    if (net::FileURLToFilePath(GURL(line), &path))
      filenames.push_back(ui::FileInfo(std::move(path), base::FilePath()));
  }
  return filenames;
}

std::string TestDataExchangeDelegate::GetMimeTypeForUriList(
    ui::EndpointType target) const {
  return "text/uri-list";
}

void TestDataExchangeDelegate::SendFileInfo(
    ui::EndpointType target,
    const std::vector<ui::FileInfo>& files,
    SendDataCallback callback) const {
  std::vector<std::string> lines;
  for (const auto& file : files) {
    lines.push_back("file://" + file.path.value());
  }
  std::string result = base::JoinString(lines, "\r\n");
  std::move(callback).Run(base::RefCountedString::TakeString(&result));
}

bool TestDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  return true;
}

void TestDataExchangeDelegate::SendPickle(ui::EndpointType target,
                                          const base::Pickle& pickle,
                                          SendDataCallback callback) {
  send_pickle_callback_ = std::move(callback);
}

void TestDataExchangeDelegate::RunSendPickleCallback(std::vector<GURL> urls) {
  std::vector<std::string> lines;
  for (const auto& url : urls) {
    lines.push_back(url.spec());
  }
  std::string result = base::JoinString(lines, "\r\n");
  std::move(send_pickle_callback_)
      .Run(base::RefCountedString::TakeString(&result));
}

std::vector<ui::FileInfo> TestDataExchangeDelegate::ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle) const {
  std::vector<ui::FileInfo> file_info;
  std::string lines(static_cast<const char*>(pickle.data()), pickle.size());
  for (const base::StringPiece& line : base::SplitStringPiece(
           lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path;
    if (net::FileURLToFilePath(GURL(line), &path))
      file_info.push_back(ui::FileInfo(std::move(path), base::FilePath()));
  }
  return file_info;
}

}  // namespace exo
