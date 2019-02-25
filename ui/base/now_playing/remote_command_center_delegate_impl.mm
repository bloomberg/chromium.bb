// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/now_playing/remote_command_center_delegate_impl.h"

#include "ui/base/now_playing/remote_command_center_delegate_ns.h"

namespace now_playing {

// static
RemoteCommandCenterDelegateImpl* RemoteCommandCenterDelegateImpl::instance_ =
    nullptr;

// static
RemoteCommandCenterDelegateImpl*
RemoteCommandCenterDelegateImpl::GetInstance() {
  return instance_;
}

// static
std::unique_ptr<RemoteCommandCenterDelegate>
RemoteCommandCenterDelegate::Create() {
  if (@available(macOS 10.12.2, *))
    return std::make_unique<RemoteCommandCenterDelegateImpl>();
  return nullptr;
}

// static
RemoteCommandCenterDelegate* RemoteCommandCenterDelegate::GetInstance() {
  if (@available(macOS 10.12.2, *))
    return RemoteCommandCenterDelegateImpl::GetInstance();
  return nullptr;
}

RemoteCommandCenterDelegateImpl::RemoteCommandCenterDelegateImpl() {
  DCHECK_EQ(nullptr, instance_);
  instance_ = this;
  remote_command_center_delegate_ns_.reset(
      [[RemoteCommandCenterDelegateNS alloc] initWithDelegate:this]);
}

RemoteCommandCenterDelegateImpl::~RemoteCommandCenterDelegateImpl() {
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;

  // Ensure that we unregister from all commands.
  SetCanPlay(false);
  SetCanPause(false);
  SetCanStop(false);
  SetCanPlayPause(false);
  SetCanGoNextTrack(false);
  SetCanGoPreviousTrack(false);
}

void RemoteCommandCenterDelegateImpl::AddObserver(
    RemoteCommandCenterDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void RemoteCommandCenterDelegateImpl::RemoveObserver(
    RemoteCommandCenterDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RemoteCommandCenterDelegateImpl::SetCanPlay(bool enable) {
  if (!ShouldSetCommandActive(Command::kPlay, enable))
    return;

  [remote_command_center_delegate_ns_ setCanPlay:enable];
}
void RemoteCommandCenterDelegateImpl::SetCanPause(bool enable) {
  if (!ShouldSetCommandActive(Command::kPause, enable))
    return;

  [remote_command_center_delegate_ns_ setCanPause:enable];
}
void RemoteCommandCenterDelegateImpl::SetCanStop(bool enable) {
  if (!ShouldSetCommandActive(Command::kStop, enable))
    return;

  [remote_command_center_delegate_ns_ setCanStop:enable];
}
void RemoteCommandCenterDelegateImpl::SetCanPlayPause(bool enable) {
  if (!ShouldSetCommandActive(Command::kPlayPause, enable))
    return;

  [remote_command_center_delegate_ns_ setCanPlayPause:enable];
}
void RemoteCommandCenterDelegateImpl::SetCanGoNextTrack(bool enable) {
  if (!ShouldSetCommandActive(Command::kNextTrack, enable))
    return;

  [remote_command_center_delegate_ns_ setCanGoNextTrack:enable];
}
void RemoteCommandCenterDelegateImpl::SetCanGoPreviousTrack(bool enable) {
  if (!ShouldSetCommandActive(Command::kPreviousTrack, enable))
    return;

  [remote_command_center_delegate_ns_ setCanGoPreviousTrack:enable];
}

void RemoteCommandCenterDelegateImpl::OnNext() {
  DCHECK(active_commands_.contains(Command::kNextTrack));
  for (RemoteCommandCenterDelegateObserver& observer : observers_)
    observer.OnNext();
}
void RemoteCommandCenterDelegateImpl::OnPrevious() {
  DCHECK(active_commands_.contains(Command::kPreviousTrack));
  for (RemoteCommandCenterDelegateObserver& observer : observers_)
    observer.OnPrevious();
}
void RemoteCommandCenterDelegateImpl::OnPause() {
  DCHECK(active_commands_.contains(Command::kPause));
  for (RemoteCommandCenterDelegateObserver& observer : observers_)
    observer.OnPause();
}
void RemoteCommandCenterDelegateImpl::OnPlayPause() {
  DCHECK(active_commands_.contains(Command::kPlayPause));
  for (RemoteCommandCenterDelegateObserver& observer : observers_)
    observer.OnPlayPause();
}
void RemoteCommandCenterDelegateImpl::OnStop() {
  DCHECK(active_commands_.contains(Command::kStop));
  for (RemoteCommandCenterDelegateObserver& observer : observers_)
    observer.OnStop();
}
void RemoteCommandCenterDelegateImpl::OnPlay() {
  DCHECK(active_commands_.contains(Command::kPlay));
  for (RemoteCommandCenterDelegateObserver& observer : observers_)
    observer.OnPlay();
}

bool RemoteCommandCenterDelegateImpl::ShouldSetCommandActive(Command command,
                                                             bool will_enable) {
  if (will_enable == active_commands_.contains(command))
    return false;

  if (will_enable)
    active_commands_.insert(command);
  else
    active_commands_.erase(command);

  return true;
}

}  // namespace now_playing
