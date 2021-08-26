// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_STREAM_ADAPTER_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_STREAM_ADAPTER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"

namespace mojo {
struct HandleSignalsState;
class SimpleWatcher;
}  // namespace mojo

namespace arc {

// ShareInfoFileStreamAdapter will stream read data based on the provided
// FileSystemURL container for a file in a ARC virtual filesystem (not
// backed by the Linux VFS). It will convert the transferred data by writing to
// a scoped file descriptor or mojo data pipe. This class is instantiated and
// lives on the UI thread. Internal file stream functions and object destruction
// is on the IO thread. Errors that occur mid-stream will abort and cleanup.
class ShareInfoFileStreamAdapter
    : public base::RefCountedThreadSafe<
          ShareInfoFileStreamAdapter,
          content::BrowserThread::DeleteOnIOThread> {
 public:
  // |result| denotes whether the requested size of data in bytes was streamed.
  using ResultCallback = base::OnceCallback<void(bool result)>;

  // Constructor used when streaming into file descriptor.
  ShareInfoFileStreamAdapter(scoped_refptr<storage::FileSystemContext> context,
                             const storage::FileSystemURL& url,
                             int64_t offset,
                             int64_t file_size,
                             int buf_size,
                             base::ScopedFD dest_fd,
                             ResultCallback result_callback);

  // Constructor used when streaming into mojo data pipe.
  ShareInfoFileStreamAdapter(scoped_refptr<storage::FileSystemContext> context,
                             const storage::FileSystemURL& url,
                             int64_t offset,
                             int64_t file_size,
                             int buf_size,
                             mojo::ScopedDataPipeProducerHandle producer_stream,
                             ResultCallback result_callback);

  ShareInfoFileStreamAdapter(const ShareInfoFileStreamAdapter&) = delete;
  ShareInfoFileStreamAdapter& operator=(const ShareInfoFileStreamAdapter&) =
      delete;

  // Create and start task runner for blocking IO threads.
  void StartRunner();

  // Used only for testing.
  void StartRunnerForTesting();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<ShareInfoFileStreamAdapter>;

  ~ShareInfoFileStreamAdapter();

  // Used to stream content to file.
  void StartFileStreaming();
  void PerformReadFileStream();
  void WriteToFile(int bytes_read);

  // Called by |simple_watcher_| to notify that |producer_stream_|'s state has
  // changed and streaming should either resume or cancel.
  void OnProducerStreamUpdate(MojoResult result,
                              const mojo::HandleSignalsState& state);
  void WriteToPipe();

  // Called when intermediate read transfers actual size of |bytes_read|.
  void OnReadFile(int bytes_read);

  // Called when intermediate write is finished.
  void OnWriteFinished(bool result);

  // Called when all transactions are completed to notify caller.
  void OnStreamingFinished(bool result);

  scoped_refptr<storage::FileSystemContext> context_;
  const storage::FileSystemURL url_;
  const int64_t offset_;
  int64_t bytes_remaining_;
  base::ScopedFD dest_fd_;
  mojo::ScopedDataPipeProducerHandle producer_stream_;
  ResultCallback result_callback_;

  std::unique_ptr<storage::FileStreamReader> stream_reader_;
  // Use task runner for blocking IO.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<net::IOBufferWithSize> net_iobuf_;

  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  // Size of the pending write in |net_iobuf_|, in bytes, used when writing to a
  // pipe.
  uint32_t pipe_write_size_;
  uint32_t pipe_write_offset_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShareInfoFileStreamAdapter> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_STREAM_ADAPTER_H_
