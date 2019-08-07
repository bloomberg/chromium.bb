// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_MHTML_HELPER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_MHTML_HELPER_H_

#include "content/browser/devtools/protocol/page_handler.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace content {
namespace protocol {

class DevToolsMHTMLHelper
    : public base::RefCountedThreadSafe<DevToolsMHTMLHelper> {
 public:
  static void Capture(
      base::WeakPtr<PageHandler> page_handler,
      std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback);

 private:
  DevToolsMHTMLHelper(
      base::WeakPtr<PageHandler> page_handler,
      std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback);
  ~DevToolsMHTMLHelper();

  void CreateTemporaryFile();
  void TemporaryFileCreatedOnIO();
  void TemporaryFileCreatedOnUI();
  void MHTMLGeneratedOnUI(int64_t mhtml_file_size);
  void ReadMHTML();
  void ReportFailure(const std::string& message);
  void ReportSuccess(std::unique_ptr<std::string> mhtml_snapshot);

  base::WeakPtr<PageHandler> page_handler_;
  std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback_;
  scoped_refptr<storage::ShareableFileReference> mhtml_file_;
  base::FilePath mhtml_snapshot_path_;

  friend class base::RefCountedThreadSafe<DevToolsMHTMLHelper>;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_MHTML_HELPER_H_
