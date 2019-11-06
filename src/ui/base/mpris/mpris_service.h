// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MPRIS_MPRIS_SERVICE_H_
#define UI_BASE_MPRIS_MPRIS_SERVICE_H_

#include <string>

#include "base/component_export.h"
#include "base/strings/string16.h"

namespace mpris {

class MprisServiceObserver;

COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIServiceNamePrefix[];
COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIObjectPath[];
COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIInterfaceName[];
COMPONENT_EXPORT(MPRIS) extern const char kMprisAPIPlayerInterfaceName[];

// A D-Bus service conforming to the MPRIS spec:
// https://specifications.freedesktop.org/mpris-spec/latest/
class COMPONENT_EXPORT(MPRIS) MprisService {
 public:
  enum class PlaybackStatus {
    kPlaying,
    kPaused,
    kStopped,
  };

  // Returns the singleton instance, creating if necessary.
  static MprisService* GetInstance();

  // Starts the DBus service.
  virtual void StartService() = 0;

  virtual void AddObserver(MprisServiceObserver* observer) = 0;
  virtual void RemoveObserver(MprisServiceObserver* observer) = 0;

  // Setters for properties.
  virtual void SetCanGoNext(bool value) = 0;
  virtual void SetCanGoPrevious(bool value) = 0;
  virtual void SetCanPlay(bool value) = 0;
  virtual void SetCanPause(bool value) = 0;
  virtual void SetPlaybackStatus(PlaybackStatus value) = 0;
  virtual void SetTitle(const base::string16& value) = 0;
  virtual void SetArtist(const base::string16& value) = 0;
  virtual void SetAlbum(const base::string16& value) = 0;

  // Returns the generated service name.
  virtual std::string GetServiceName() const = 0;

 protected:
  virtual ~MprisService();
};

}  // namespace mpris

#endif  // UI_BASE_MPRIS_MPRIS_SERVICE_H_
