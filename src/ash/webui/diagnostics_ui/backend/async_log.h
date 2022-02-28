// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ASYNC_LOG_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ASYNC_LOG_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {
namespace diagnostics {

// `AsyncLog` is a simple test logger that appends lines/strings to the end of
// a file. The file is created on first write, and all IO operations occur
// on a `SequencedTaskRunner`.
class AsyncLog {
 public:
  explicit AsyncLog(const base::FilePath& file_path);
  AsyncLog(const AsyncLog&) = delete;
  AsyncLog& operator=(const AsyncLog&) = delete;
  ~AsyncLog();

  // Appends text to the file by posting to task runner.
  void Append(const std::string& text);

  // Returns the current contents as a string.
  std::string GetContents() const;

 private:
  // Appends to the file. Run on the the task runner.
  void AppendImpl(const std::string& text);

  // Create the log file. Called on the first write to the file.
  void CreateFile();

  // Path of the log file.
  const base::FilePath file_path_;

  // Blockable task runner to enable I/O operations.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ASYNC_LOG_H_
