// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_hung_plugin_filter.h"

#include "base/bind.h"
#include "content/child/child_process.h"
#include "content/common/frame_messages.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

namespace {

// We'll consider the plugin hung after not hearing anything for this long.
const int kHungThresholdSec = 10;

// If we ever are blocked for this long, we'll consider the plugin hung, even
// if we continue to get messages (which is why the above hung threshold never
// kicked in). Maybe the plugin is spamming us with events and never unblocking
// and never processing our sync message.
const int kBlockedHardThresholdSec = kHungThresholdSec * 1.5;

}  // namespace

PepperHungPluginFilter::PepperHungPluginFilter(
    const base::FilePath& plugin_path,
    int frame_routing_id,
    int plugin_child_id)
    : plugin_path_(plugin_path),
      frame_routing_id_(frame_routing_id),
      plugin_child_id_(plugin_child_id),
      filter_(RenderThread::Get()->GetSyncMessageFilter()),
      io_task_runner_(ChildProcess::current()->io_task_runner()),
      pending_sync_message_count_(0),
      hung_plugin_showing_(false),
      timer_task_pending_(false) {
}

void PepperHungPluginFilter::BeginBlockOnSyncMessage() {
  base::AutoLock lock(lock_);
  last_message_received_ = base::TimeTicks::Now();
  if (pending_sync_message_count_ == 0)
    began_blocking_time_ = last_message_received_;
  pending_sync_message_count_++;

  EnsureTimerScheduled();
}

void PepperHungPluginFilter::EndBlockOnSyncMessage() {
  base::AutoLock lock(lock_);
  pending_sync_message_count_--;
  DCHECK(pending_sync_message_count_ >= 0);

  MayHaveBecomeUnhung();
}

void PepperHungPluginFilter::OnFilterRemoved() {
  base::AutoLock lock(lock_);
  MayHaveBecomeUnhung();
}

void PepperHungPluginFilter::OnChannelError() {
  base::AutoLock lock(lock_);
  MayHaveBecomeUnhung();
}

bool PepperHungPluginFilter::OnMessageReceived(const IPC::Message& message) {
  // Just track incoming message times but don't handle any messages.
  base::AutoLock lock(lock_);
  last_message_received_ = base::TimeTicks::Now();
  MayHaveBecomeUnhung();
  return false;
}

PepperHungPluginFilter::~PepperHungPluginFilter() {}

void PepperHungPluginFilter::EnsureTimerScheduled() {
  lock_.AssertAcquired();
  if (timer_task_pending_)
    return;

  timer_task_pending_ = true;
  io_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PepperHungPluginFilter::OnHangTimer, this),
      base::TimeDelta::FromSeconds(kHungThresholdSec));
}

void PepperHungPluginFilter::MayHaveBecomeUnhung() {
  lock_.AssertAcquired();
  if (!hung_plugin_showing_ || IsHung())
    return;

  SendHungMessage(false);
  hung_plugin_showing_ = false;
}

base::TimeTicks PepperHungPluginFilter::GetHungTime() const {
  lock_.AssertAcquired();

  DCHECK(pending_sync_message_count_);
  DCHECK(!began_blocking_time_.is_null());
  DCHECK(!last_message_received_.is_null());

  // Always considered hung at the hard threshold.
  base::TimeTicks hard_time =
      began_blocking_time_ +
      base::TimeDelta::FromSeconds(kBlockedHardThresholdSec);

  // Hung after a soft threshold from last message of any sort.
  base::TimeTicks soft_time =
      last_message_received_ + base::TimeDelta::FromSeconds(kHungThresholdSec);

  return std::min(soft_time, hard_time);
}

bool PepperHungPluginFilter::IsHung() const {
  lock_.AssertAcquired();

  if (!pending_sync_message_count_)
    return false;  // Not blocked on a sync message.

  return base::TimeTicks::Now() > GetHungTime();
}

void PepperHungPluginFilter::OnHangTimer() {
  base::AutoLock lock(lock_);
  timer_task_pending_ = false;

  if (!pending_sync_message_count_)
    return;  // Not blocked any longer.

  base::TimeDelta delay = GetHungTime() - base::TimeTicks::Now();
  if (delay > base::TimeDelta()) {
    // Got a timer message while we're waiting on a sync message. We need
    // to schedule another timer message because the latest sync message
    // would not have scheduled one (we only have one out-standing timer at
    // a time).
    timer_task_pending_ = true;
    io_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PepperHungPluginFilter::OnHangTimer, this),
        delay);
    return;
  }

  hung_plugin_showing_ = true;
  SendHungMessage(true);
}

void PepperHungPluginFilter::SendHungMessage(bool is_hung) {
  filter_->Send(new FrameHostMsg_PepperPluginHung(
      frame_routing_id_, plugin_child_id_, plugin_path_, is_hung));
}

}  // namespace content
