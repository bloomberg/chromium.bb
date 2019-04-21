// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/page_auto_fetcher_helper_android.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"

PageAutoFetcherHelper::PageAutoFetcherHelper(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}
PageAutoFetcherHelper::~PageAutoFetcherHelper() = default;

void PageAutoFetcherHelper::TrySchedule(
    bool user_requested,
    base::OnceCallback<void(FetcherScheduleResult)> complete_callback) {
  if (!Bind()) {
    std::move(complete_callback).Run(FetcherScheduleResult::kOtherError);
    return;
  }

  fetcher_->TrySchedule(
      user_requested,
      base::BindOnce(&PageAutoFetcherHelper::TryScheduleComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(complete_callback)));
}

void PageAutoFetcherHelper::TryScheduleComplete(
    base::OnceCallback<void(FetcherScheduleResult)> complete_callback,
    FetcherScheduleResult result) {
  std::move(complete_callback).Run(result);
}

void PageAutoFetcherHelper::CancelSchedule() {
  if (Bind()) {
    fetcher_->CancelSchedule();
  }
}

bool PageAutoFetcherHelper::Bind() {
  if (fetcher_)
    return true;
  render_frame_->GetRemoteInterfaces()->GetInterface(
      mojo::MakeRequest(&fetcher_));
  return fetcher_.is_bound();
}
