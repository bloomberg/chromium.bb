// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_source.h"

#include <wayland-server-core.h>

#include <string>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

namespace wl {

namespace {

void DataSourceDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void DataSourceSetActions(wl_client* client,
                          wl_resource* resource,
                          uint32_t dnd_actions) {
  GetUserDataAs<TestDataSource>(resource)->SetActions(dnd_actions);
}

struct WlDataSourceImpl : public TestSelectionSource::Delegate {
  explicit WlDataSourceImpl(TestDataSource* offer) : source_(offer) {}
  ~WlDataSourceImpl() override = default;

  WlDataSourceImpl(const WlDataSourceImpl&) = delete;
  WlDataSourceImpl& operator=(const WlDataSourceImpl&) = delete;

  void SendSend(const std::string& mime_type,
                base::ScopedFD write_fd) override {
    wl_data_source_send_send(source_->resource(), mime_type.c_str(),
                             write_fd.get());
    wl_client_flush(wl_resource_get_client(source_->resource()));
  }

  void SendCancelled() override {
    wl_data_source_send_cancelled(source_->resource());
  }

  void OnDestroying() override { delete this; }

 private:
  TestDataSource* const source_;
};

}  // namespace

const struct wl_data_source_interface kTestDataSourceImpl = {
    TestSelectionSource::Offer, DataSourceDestroy, DataSourceSetActions};

TestDataSource::TestDataSource(wl_resource* resource)
    : TestSelectionSource(resource, new WlDataSourceImpl(this)) {}

TestDataSource::~TestDataSource() = default;

void TestDataSource::SetActions(uint32_t dnd_actions) {
  actions_ |= dnd_actions;
}

}  // namespace wl
