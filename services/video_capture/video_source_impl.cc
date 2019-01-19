// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_impl.h"

#include "services/video_capture/push_video_stream_subscription_impl.h"

namespace video_capture {

VideoSourceImpl::VideoSourceImpl(
    mojom::DeviceFactory* device_factory,
    const std::string& device_id,
    base::RepeatingClosure on_last_binding_closed_cb)
    : device_factory_(device_factory),
      device_id_(device_id),
      on_last_binding_closed_cb_(std::move(on_last_binding_closed_cb)),
      device_status_(DeviceStatus::kNotStarted),
      weak_factory_(this) {
  bindings_.set_connection_error_handler(base::BindRepeating(
      &VideoSourceImpl::OnClientDisconnected, base::Unretained(this)));
}

VideoSourceImpl::~VideoSourceImpl() {
  bindings_.set_connection_error_handler(base::DoNothing());
}

void VideoSourceImpl::AddToBindingSet(mojom::VideoSourceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void VideoSourceImpl::CreatePushSubscription(
    mojom::ReceiverPtr subscriber,
    const media::VideoCaptureParams& requested_settings,
    bool force_reopen_with_new_settings,
    mojom::PushVideoStreamSubscriptionRequest subscription_request,
    CreatePushSubscriptionCallback callback) {
  if (force_reopen_with_new_settings) {
    NOTIMPLEMENTED();
    return;
  }
  auto subscription = std::make_unique<PushVideoStreamSubscriptionImpl>(
      std::move(subscription_request), std::move(subscriber),
      requested_settings, std::move(callback), &broadcaster_, &device_);
  subscription->SetOnClosedHandler(base::BindOnce(
      &VideoSourceImpl::OnPushSubscriptionClosedOrDisconnectedOrDiscarded,
      weak_factory_.GetWeakPtr(), subscription.get()));
  auto* subscription_ptr = subscription.get();
  push_subscriptions_.insert(
      std::make_pair(subscription.get(), std::move(subscription)));
  switch (device_status_) {
    case DeviceStatus::kNotStarted:
      device_start_settings_ = requested_settings;
      device_status_ = DeviceStatus::kStartingAsynchronously;
      device_factory_->CreateDevice(
          device_id_, mojo::MakeRequest(&device_),
          base::BindOnce(&VideoSourceImpl::OnCreateDeviceResponse,
                         weak_factory_.GetWeakPtr()));
      return;
    case DeviceStatus::kStartingAsynchronously:
      // No need to do anything. Response will be sent when
      // OnCreateDeviceResponse() gets called.
      return;
    case DeviceStatus::kStarted:
      subscription_ptr->NotifySubscriberCreateSubscriptionSucceededWithSettings(
          device_start_settings_);
      return;
  }
}

void VideoSourceImpl::OnClientDisconnected() {
  if (bindings_.empty()) {
    // Note: Invoking this callback may synchronously trigger the destruction of
    // |this|, so no more member access should be done after it.
    on_last_binding_closed_cb_.Run();
  }
}

void VideoSourceImpl::OnCreateDeviceResponse(
    mojom::DeviceAccessResultCode result_code) {
  switch (result_code) {
    case mojom::DeviceAccessResultCode::SUCCESS: {
      mojom::ReceiverPtr broadcaster_as_receiver;
      broadcaster_binding_ = std::make_unique<mojo::Binding<mojom::Receiver>>(
          &broadcaster_, mojo::MakeRequest(&broadcaster_as_receiver));
      device_->Start(device_start_settings_,
                     std::move(broadcaster_as_receiver));
      device_status_ = DeviceStatus::kStarted;
      if (push_subscriptions_.empty()) {
        StopDevice();
        return;
      }
      for (auto& entry : push_subscriptions_) {
        auto& subscription = entry.second;
        subscription->NotifySubscriberCreateSubscriptionSucceededWithSettings(
            device_start_settings_);
      }
      return;
    }
    case mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:  // Fall through
    case mojom::DeviceAccessResultCode::NOT_INITIALIZED:
      for (auto& entry : push_subscriptions_) {
        auto& subscription = entry.second;
        subscription->NotifySubscriberCreateSubscriptionFailed();
      }
      push_subscriptions_.clear();
      device_status_ = DeviceStatus::kNotStarted;
      return;
  }
}

void VideoSourceImpl::OnPushSubscriptionClosedOrDisconnectedOrDiscarded(
    PushVideoStreamSubscriptionImpl* subscription,
    base::OnceClosure done_cb) {
  // We keep the subscription instance alive until after having called |done_cb|
  // in order to allow it to send out a callback before being destroyed.
  auto subscription_ownership = std::move(push_subscriptions_[subscription]);
  push_subscriptions_.erase(subscription);
  if (push_subscriptions_.empty()) {
    switch (device_status_) {
      case DeviceStatus::kNotStarted:
        // Nothing to do here.
        break;
      case DeviceStatus::kStartingAsynchronously:
        // We will check again in OnCreateDeviceResponse() whether or not there
        // are any subscriptions.
        break;
      case DeviceStatus::kStarted:
        StopDevice();
        break;
    }
  }
  std::move(done_cb).Run();
}

void VideoSourceImpl::StopDevice() {
  device_.reset();
  device_status_ = DeviceStatus::kNotStarted;
}

}  // namespace video_capture
