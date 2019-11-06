// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_IMPL_H_
#define UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_IMPL_H_

#include <windows.foundation.h>
#include <windows.media.control.h>
#include <wrl/client.h>

#include "base/component_export.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/base/win/system_media_controls/system_media_controls_service.h"

namespace system_media_controls {

class SystemMediaControlsServiceObserver;

namespace internal {

// Implementation of SystemMediaControlsService that actually connects to the
// Windows's System Media Transport Controls.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsServiceImpl
    : public SystemMediaControlsService {
 public:
  SystemMediaControlsServiceImpl();
  ~SystemMediaControlsServiceImpl() override;

  static SystemMediaControlsServiceImpl* GetInstance();

  // Connects to the SystemMediaTransportControls. Returns true if connection
  // is successful. If already connected, does nothing and returns true.
  bool Initialize();

  // SystemMediaControlsService implementation.
  void AddObserver(SystemMediaControlsServiceObserver* observer) override;
  void RemoveObserver(SystemMediaControlsServiceObserver* observer) override;
  void SetEnabled(bool enabled) override;
  void SetIsNextEnabled(bool value) override;
  void SetIsPreviousEnabled(bool value) override;
  void SetIsPlayEnabled(bool value) override;
  void SetIsPauseEnabled(bool value) override;
  void SetIsStopEnabled(bool value) override;
  void SetPlaybackStatus(
      ABI::Windows::Media::MediaPlaybackStatus status) override;
  void SetTitle(const base::string16& title) override;
  void SetArtist(const base::string16& artist) override;
  void SetThumbnail(const SkBitmap& bitmap) override;
  void ClearThumbnail() override;
  void ClearMetadata() override;
  void UpdateDisplay() override;

 private:
  friend struct base::DefaultSingletonTraits<SystemMediaControlsServiceImpl>;

  static HRESULT ButtonPressed(
      ABI::Windows::Media::ISystemMediaTransportControls* sender,
      ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs*
          args);

  // Called by ButtonPressed when the particular key is pressed.
  void OnPlay();
  void OnPause();
  void OnNext();
  void OnPrevious();
  void OnStop();

  // Control and keep track of the metadata.
  Microsoft::WRL::ComPtr<ABI::Windows::Media::ISystemMediaTransportControls>
      system_media_controls_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater>
      display_updater_;
  Microsoft::WRL::ComPtr<ABI::Windows::Media::IMusicDisplayProperties>
      display_properties_;
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IDataWriter>
      icon_data_writer_;
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream>
      icon_stream_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Storage::Streams::IRandomAccessStreamReference>
      icon_stream_reference_;

  EventRegistrationToken registration_token_;

  // True if we've already tried to connect to the SystemMediaTransportControls.
  bool attempted_to_initialize_ = false;

  // True if we've successfully registered a button handler on the
  // SystemMediaTransportControls.
  bool has_valid_registration_token_ = false;

  // True if we've successfully connected to the SystemMediaTransportControls.
  bool initialized_ = false;

  base::ObserverList<SystemMediaControlsServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SystemMediaControlsServiceImpl);
};

}  // namespace internal

}  // namespace system_media_controls

#endif  // UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_IMPL_H_
