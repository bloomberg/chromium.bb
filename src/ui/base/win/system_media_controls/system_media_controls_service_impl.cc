// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/system_media_controls/system_media_controls_service_impl.h"

#include <systemmediatransportcontrolsinterop.h>
#include <windows.media.control.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include "base/strings/string_piece.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "ui/base/win/system_media_controls/system_media_controls_service_observer.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace system_media_controls {

namespace internal {

using ABI::Windows::Media::ISystemMediaTransportControls;
using ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs;
using ABI::Windows::Media::SystemMediaTransportControls;
using ABI::Windows::Media::SystemMediaTransportControlsButton;
using ABI::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs;

// static
SystemMediaControlsServiceImpl* SystemMediaControlsServiceImpl::GetInstance() {
  // We use a base::Singleton here instead of a base::NoDestruct so that we can
  // clean up external listeners against the Windows platform at exit.
  return base::Singleton<SystemMediaControlsServiceImpl>::get();
}

SystemMediaControlsServiceImpl::SystemMediaControlsServiceImpl() = default;

SystemMediaControlsServiceImpl::~SystemMediaControlsServiceImpl() {
  if (has_valid_registration_token_) {
    DCHECK(system_media_controls_);
    system_media_controls_->remove_ButtonPressed(registration_token_);
    system_media_controls_->put_IsEnabled(false);
  }
}

bool SystemMediaControlsServiceImpl::Initialize() {
  if (attempted_to_initialize_)
    return initialized_;

  attempted_to_initialize_ = true;

  if (!base::win::ResolveCoreWinRTDelayload() ||
      !base::win::ScopedHString::ResolveCoreWinRTStringDelayload()) {
    return false;
  }

  Microsoft::WRL::ComPtr<ISystemMediaTransportControlsInterop> interop;
  HRESULT hr = base::win::GetActivationFactory<
      ISystemMediaTransportControlsInterop,
      RuntimeClass_Windows_Media_SystemMediaTransportControls>(&interop);
  if (FAILED(hr))
    return false;

  hr = interop->GetForWindow(gfx::SingletonHwnd::GetInstance()->hwnd(),
                             IID_PPV_ARGS(&system_media_controls_));
  if (FAILED(hr))
    return false;

  auto handler =
      Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
          SystemMediaTransportControls*,
          SystemMediaTransportControlsButtonPressedEventArgs*>>(
          &SystemMediaControlsServiceImpl::ButtonPressed);
  hr = system_media_controls_->add_ButtonPressed(handler.Get(),
                                                 &registration_token_);
  if (FAILED(hr))
    return false;

  has_valid_registration_token_ = true;

  hr = system_media_controls_->put_IsEnabled(true);
  if (FAILED(hr))
    return false;

  initialized_ = true;
  return true;
}

void SystemMediaControlsServiceImpl::AddObserver(
    SystemMediaControlsServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void SystemMediaControlsServiceImpl::RemoveObserver(
    SystemMediaControlsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SystemMediaControlsServiceImpl::SetIsNextEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsNextEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsServiceImpl::SetIsPreviousEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsPreviousEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsServiceImpl::SetIsPlayEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsPlayEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsServiceImpl::SetIsPauseEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsPauseEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsServiceImpl::SetIsStopEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsStopEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsServiceImpl::SetPlaybackStatus(
    ABI::Windows::Media::MediaPlaybackStatus status) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_PlaybackStatus(status);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsServiceImpl::OnPlay() {
  for (SystemMediaControlsServiceObserver& obs : observers_)
    obs.OnPlay();
}

void SystemMediaControlsServiceImpl::OnPause() {
  for (SystemMediaControlsServiceObserver& obs : observers_)
    obs.OnPause();
}

void SystemMediaControlsServiceImpl::OnNext() {
  for (SystemMediaControlsServiceObserver& obs : observers_)
    obs.OnNext();
}

void SystemMediaControlsServiceImpl::OnPrevious() {
  for (SystemMediaControlsServiceObserver& obs : observers_)
    obs.OnPrevious();
}

void SystemMediaControlsServiceImpl::OnStop() {
  for (SystemMediaControlsServiceObserver& obs : observers_)
    obs.OnStop();
}

// static
HRESULT SystemMediaControlsServiceImpl::ButtonPressed(
    ISystemMediaTransportControls* sender,
    ISystemMediaTransportControlsButtonPressedEventArgs* args) {
  SystemMediaTransportControlsButton button;
  HRESULT hr = args->get_Button(&button);
  if (FAILED(hr))
    return hr;

  SystemMediaControlsServiceImpl* impl = GetInstance();

  switch (button) {
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Play:
      impl->OnPlay();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Pause:
      impl->OnPause();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Next:
      impl->OnNext();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Previous:
      impl->OnPrevious();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Stop:
      impl->OnStop();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Record:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_FastForward:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Rewind:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_ChannelUp:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_ChannelDown:
      break;
  }

  return S_OK;
}

}  // namespace internal

}  // namespace system_media_controls
