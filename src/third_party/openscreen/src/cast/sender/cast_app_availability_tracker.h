// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CAST_APP_AVAILABILITY_TRACKER_H_
#define CAST_SENDER_CAST_APP_AVAILABILITY_TRACKER_H_

#include <map>
#include <string>
#include <vector>

#include "cast/sender/channel/message_util.h"
#include "cast/sender/public/cast_media_source.h"
#include "platform/api/time.h"

namespace openscreen {
namespace cast {

// Tracks device queries and their extracted Cast app IDs and their
// availabilities on discovered devices.
// Example usage:
///
// (1) A page is interested in a Cast URL (e.g. by creating a
// PresentationRequest with the URL) like "cast:foo". To register the source to
// be tracked:
//   CastAppAvailabilityTracker tracker;
//   auto source = CastMediaSource::From("cast:foo");
//   auto new_app_ids = tracker.RegisterSource(source.value());
//
// (2) The set of app IDs returned by the tracker can then be used by the caller
// to send an app availability request to each of the discovered devices.
//
// (3) Once the caller knows the availability value for a (device, app) pair, it
// may inform the tracker to update its results:
//   auto affected_sources =
//       tracker.UpdateAppAvailability(device_id, app_id, {availability, now});
//
// (4) The tracker returns a subset of discovered sources that were affected by
// the update. The caller can then call |GetAvailableDevices()| to get the
// updated results for each affected source.
//
// (5a): At any time, the caller may call |RemoveResultsForDevice()| to remove
// cached results pertaining to the device, when it detects that a device is
// removed or no longer valid.
//
// (5b): At any time, the caller may call |GetAvailableDevices()| (even before
// the source is registered) to determine if there are cached results available.
// TODO(crbug.com/openscreen/112): Device -> Receiver renaming.
class CastAppAvailabilityTracker {
 public:
  // The result of an app availability request and the time when it is obtained.
  struct AppAvailability {
    AppAvailabilityResult availability;
    Clock::time_point time;
  };

  CastAppAvailabilityTracker();
  ~CastAppAvailabilityTracker();

  CastAppAvailabilityTracker(const CastAppAvailabilityTracker&) = delete;
  CastAppAvailabilityTracker& operator=(const CastAppAvailabilityTracker&) =
      delete;

  // Registers |source| with the tracker. Returns a list of new app IDs that
  // were previously not known to the tracker.
  std::vector<std::string> RegisterSource(const CastMediaSource& source);

  // Unregisters the source given by |source| or |source_id| with the tracker.
  void UnregisterSource(const std::string& source_id);
  void UnregisterSource(const CastMediaSource& source);

  // Updates the availability of |app_id| on |device_id| to |availability|.
  // Returns a list of registered CastMediaSources for which the set of
  // available devices might have been updated by this call. The caller should
  // call |GetAvailableDevices| with the returned CastMediaSources to get the
  // updated lists.
  std::vector<CastMediaSource> UpdateAppAvailability(
      const std::string& device_id,
      const std::string& app_id,
      AppAvailability availability);

  // Removes all results associated with |device_id|, i.e. when the device
  // becomes invalid.  Returns a list of registered CastMediaSources for which
  // the set of available devices might have been updated by this call. The
  // caller should call |GetAvailableDevices| with the returned CastMediaSources
  // to get the updated lists.
  std::vector<CastMediaSource> RemoveResultsForDevice(
      const std::string& device_id);

  // Returns a list of registered CastMediaSources supported by |device_id|.
  std::vector<CastMediaSource> GetSupportedSources(
      const std::string& device_id) const;

  // Returns the availability for |app_id| on |device_id| and the time at which
  // the availability was determined. If availability is kUnknown, then the time
  // may be null (e.g. if an availability request was never sent).
  AppAvailability GetAvailability(const std::string& device_id,
                                  const std::string& app_id) const;

  // Returns a list of registered app IDs.
  std::vector<std::string> GetRegisteredApps() const;

  // Returns a list of device IDs compatible with |source|, using the current
  // availability info.
  std::vector<std::string> GetAvailableDevices(
      const CastMediaSource& source) const;

 private:
  // App ID to availability.
  using AppAvailabilityMap = std::map<std::string, AppAvailability>;

  // Registered sources and corresponding CastMediaSources.
  std::map<std::string, CastMediaSource> registered_sources_;

  // App IDs tracked and the number of registered sources containing them.
  std::map<std::string, int> registration_count_by_app_id_;

  // IDs and app availabilities of known devices.
  std::map<std::string, AppAvailabilityMap> app_availabilities_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_SENDER_CAST_APP_AVAILABILITY_TRACKER_H_
