// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"

namespace base {
class SequencedTaskRunner;
}

namespace gfx {
class Image;
}

class AccountId;
class PrefService;
class ProfileAttributesEntry;
class ProfileAvatarDownloader;

class ProfileAttributesStorage
    : public base::SupportsWeakPtr<ProfileAttributesStorage> {
 public:
  using Observer = ProfileInfoCacheObserver;

  explicit ProfileAttributesStorage(PrefService* prefs);
  ProfileAttributesStorage(const ProfileAttributesStorage&) = delete;
  ProfileAttributesStorage& operator=(const ProfileAttributesStorage&) = delete;
  virtual ~ProfileAttributesStorage();

  // Adds a new profile with `params` to the attributes storage.
  // `params.profile_path` must be a valid path within the user data directory
  // that hasn't been registered with this `ProfileAttributesStorage` before.
  virtual void AddProfile(ProfileAttributesInitParams params) = 0;

  // Removes the profile matching given |account_id| from this storage.
  // Calculates profile path and calls RemoveProfile() on it.
  virtual void RemoveProfileByAccountId(const AccountId& account_id) = 0;

  // Removes the profile at |profile_path| from this storage. Does not delete or
  // affect the actual profile's data.
  virtual void RemoveProfile(const base::FilePath& profile_path) = 0;

  // Returns a vector containing one attributes entry per known profile. They
  // are not sorted in any particular order.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributes(
      bool include_guest_profile = false);

  // Returns all non-Guest profile attributes sorted by name.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSortedByName();

  // Returns all non-Guest profile attributes sorted by local profile name.
  std::vector<ProfileAttributesEntry*>
  GetAllProfilesAttributesSortedByLocalProfilName();

  // Returns a ProfileAttributesEntry with the data for the profile at |path|
  // if the operation is successful. Returns |nullptr| otherwise.
  // Returned value should not be cached because the profile entry may be
  // deleted at any time, an then using this value would cause use-after-free.
  virtual ProfileAttributesEntry* GetProfileAttributesWithPath(
      const base::FilePath& path) = 0;

  // Returns the count of known profiles.
  virtual size_t GetNumberOfProfiles(
      bool include_guest_profile = false) const = 0;

  // Returns a unique name that can be assigned to a newly created profile.
  std::u16string ChooseNameForNewProfile(size_t icon_index) const;

  // Determines whether |name| is one of the default assigned names.
  // On Desktop, if |include_check_for_legacy_profile_name| is false,
  // |IsDefaultProfileName()| would only return true if the |name| is in the
  // form of |Person %n| which is the new default local profile name. If
  // |include_check_for_legacy_profile_name| is true, we will also check if name
  // is one of the legacy profile names (e.g. Saratoga, Default user, ..).
  // For other platforms, so far |include_check_for_legacy_profile_name|
  // is not used.
  bool IsDefaultProfileName(const std::u16string& name,
                            bool include_check_for_legacy_profile_name) const;

#if !defined(OS_ANDROID)
  // Records statistics about a profile `entry` that is being deleted. If the
  // profile has opened browser window(s) in the moment of deletion, this
  // function must be called before these windows get closed.
  void RecordDeletedProfileState(ProfileAttributesEntry* entry);
#endif

  // Records statistics about profiles as would be visible in the profile picker
  // (if we would display it in this moment).
  void RecordProfilesState();

  // Returns an avatar icon index that can be assigned to a newly created
  // profile. Note that the icon may not be unique since there are a limited
  // set of default icons.
  size_t ChooseAvatarIconIndexForNewProfile() const;

  // Returns the decoded image at |image_path|. Used both by the GAIA profile
  // image and the high res avatars.
  const gfx::Image* LoadAvatarPictureFromPath(
      const base::FilePath& profile_path,
      const std::string& key,
      const base::FilePath& image_path) const;

  // Checks whether the high res avatar at index |icon_index| exists, and if it
  // does not, calls |DownloadHighResAvatar|.
  void DownloadHighResAvatarIfNeeded(size_t icon_index,
                                     const base::FilePath& profile_path);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool GetDisableAvatarDownloadForTesting() {
    return disable_avatar_download_for_testing_;
  }

  void set_disable_avatar_download_for_testing(
      bool disable_avatar_download_for_testing) {
    disable_avatar_download_for_testing_ = disable_avatar_download_for_testing;
  }

  // Notifies observers. The following methods are accessed by
  // ProfileAttributesEntry.
  void NotifyOnProfileAvatarChanged(const base::FilePath& profile_path) const;

  // Disables the periodic reporting of profile metrics, as this is causing
  // tests to time out.
  virtual void DisableProfileMetricsForTesting() {}

 protected:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, EntriesInAttributesStorage);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           DownloadHighResAvatarTest);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           NothingToDownloadHighResAvatarTest);

  // Starts downloading the high res avatar at index |icon_index| for profile
  // with path |profile_path|.
  void DownloadHighResAvatar(size_t icon_index,
                             const base::FilePath& profile_path);

  // Saves the avatar |image| at |image_path|. This is used both for the GAIA
  // profile pictures and the ProfileAvatarDownloader that is used to download
  // the high res avatars.
  void SaveAvatarImageAtPath(const base::FilePath& profile_path,
                             gfx::Image image,
                             const std::string& key,
                             const base::FilePath& image_path,
                             base::OnceClosure callback);

  PrefService* const prefs_;
  mutable std::unordered_map<base::FilePath::StringType,
                             std::unique_ptr<ProfileAttributesEntry>>
      profile_attributes_entries_;

  mutable base::ObserverList<Observer>::Unchecked observer_list_;

  // A cache of gaia/high res avatar profile pictures. This cache is updated
  // lazily so it needs to be mutable.
  mutable std::unordered_map<std::string, gfx::Image> cached_avatar_images_;

  // Marks a profile picture as loading from disk. This prevents a picture from
  // loading multiple times.
  mutable std::unordered_map<std::string, bool> cached_avatar_images_loading_;

  // Hash table of profile pictures currently being downloaded from the remote
  // location and the ProfileAvatarDownloader instances downloading them.
  // This prevents a picture from being downloaded multiple times. The
  // ProfileAvatarDownloader instances are deleted when the download completes
  // or when the ProfileInfoCache is destroyed.
  std::unordered_map<std::string, std::unique_ptr<ProfileAvatarDownloader>>
      avatar_images_downloads_in_progress_;

  // Determines of the ProfileAvatarDownloader should be created and executed
  // or not. Only set to true for tests.
  bool disable_avatar_download_for_testing_ = false;

  // Task runner used for file operation on avatar images.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

 private:
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSorted(
      bool use_local_profile_name);

  // Called when the picture given by |key| has been loaded from disk and
  // decoded into |image|.
  void OnAvatarPictureLoaded(const base::FilePath& profile_path,
                             const std::string& key,
                             gfx::Image image) const;

  // Called when the picture given by |file_name| has been saved to disk. Used
  // both for the GAIA profile picture and the high res avatar files.
  void OnAvatarPictureSaved(const std::string& file_name,
                            const base::FilePath& profile_path,
                            base::OnceClosure callback,
                            bool success) const;

  // Helper function that calls SaveAvatarImageAtPath without a callback.
  void SaveAvatarImageAtPathNoCallback(const base::FilePath& profile_path,
                                       gfx::Image image,
                                       const std::string& key,
                                       const base::FilePath& image_path);

  // Notifies observers.
  void NotifyOnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) const;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
