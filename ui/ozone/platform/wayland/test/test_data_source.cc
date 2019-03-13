// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_source.h"

#include <wayland-server-core.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/test/constants.h"

namespace wl {

namespace {

std::vector<uint8_t> ReadDataOnWorkerThread(base::ScopedFD fd) {
  constexpr size_t kChunkSize = 1024;
  std::vector<uint8_t> bytes;
  while (true) {
    uint8_t chunk[kChunkSize];
    ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), chunk, kChunkSize));
    if (bytes_read > 0) {
      bytes.insert(bytes.end(), chunk, chunk + bytes_read);
      continue;
    }
    if (!bytes_read)
      return bytes;
    if (bytes_read < 0) {
      LOG(ERROR) << "Failed to read selection data from clipboard.";
      return std::vector<uint8_t>();
    }
  }
}

void CreatePipe(base::ScopedFD* read_pipe, base::ScopedFD* write_pipe) {
  int raw_pipe[2];
  PCHECK(0 == pipe(raw_pipe));
  read_pipe->reset(raw_pipe[0]);
  write_pipe->reset(raw_pipe[1]);
}

void DataSourceOffer(wl_client* client,
                     wl_resource* resource,
                     const char* mime_type) {
  GetUserDataAs<TestDataSource>(resource)->Offer(mime_type);
}

void DataSourceDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void SetActions(wl_client* client,
                wl_resource* resource,
                uint32_t dnd_actions) {
  NOTIMPLEMENTED();
}

}  // namespace

const struct wl_data_source_interface kTestDataSourceImpl = {
    DataSourceOffer, DataSourceDestroy, SetActions};

TestDataSource::TestDataSource(wl_resource* resource)
    : ServerObject(resource),
      task_runner_(
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})),
      read_data_weak_ptr_factory_(this) {}

TestDataSource::~TestDataSource() {}

void TestDataSource::Offer(const std::string& mime_type) {
  NOTIMPLEMENTED();
}

void TestDataSource::ReadData(ReadDataCallback callback) {
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  CreatePipe(&read_fd, &write_fd);

  // 1. Send the SEND event to notify client's DataSource that it's time
  // to send us the drag data thrhough the write_fd file descriptor.
  wl_data_source_send_send(resource(), kTextMimeTypeUtf8, write_fd.get());
  wl_client_flush(wl_resource_get_client(resource()));

  // 2. Schedule the ReadDataOnWorkerThread task. The result of read
  // operation will be delivered through TestDataSource::DataReadCb,
  // which will then call the callback function requested by the caller.
  PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadDataOnWorkerThread, std::move(read_fd)),
      base::BindOnce(&TestDataSource::DataReadCb,
                     read_data_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void TestDataSource::DataReadCb(ReadDataCallback callback,
                                const std::vector<uint8_t>& data) {
  std::move(callback).Run(data);
}

void TestDataSource::OnCancelled() {
  wl_data_source_send_cancelled(resource());
}

}  // namespace wl
