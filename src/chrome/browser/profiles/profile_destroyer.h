// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_

#include <stdint.h>

#include <set>

#include "base/memory/ref_counted.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

class Profile;
class ProfileImpl;

// We use this class to destroy the off the record profile so that we can make
// sure it gets done asynchronously after all render process hosts are gone.
class ProfileDestroyer : public content::RenderProcessHostObserver {
 public:
  // Destroys the given profile either instantly, or after a short delay waiting
  // for dependent renderer process hosts to destroy.
  // Ownership of the profile is passed to profile destroyer and the profile
  // should not be used after this call.
  static void DestroyProfileWhenAppropriate(Profile* const profile);
  ProfileDestroyer(const ProfileDestroyer&) = delete;
  ProfileDestroyer& operator=(const ProfileDestroyer&) = delete;

 private:
  friend class ProfileImpl;
  typedef std::set<content::RenderProcessHost*> HostSet;
  typedef std::set<ProfileDestroyer*> DestroyerSet;

  friend class base::RefCounted<ProfileDestroyer>;

  ProfileDestroyer(Profile* const profile, HostSet* hosts);
  ~ProfileDestroyer() override;

  // content::RenderProcessHostObserver override.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Called by the timer to cancel the pending destruction and do it now.
  void DestroyProfile();

  // Fetch the list of render process hosts that still point to |profile_ptr|.
  // |profile_ptr| is a void* because the Profile object may be freed. Only
  // pointer comparison is allowed, it will never be dereferenced as a Profile.
  //
  // If |include_spare_rph| is true, include spare render process hosts in the
  // output.
  static HostSet GetHostsForProfile(void* const profile_ptr,
                                    bool include_spare_rph = false);

  // Destroys an Original (non-off-the-record) profile immediately.
  static void DestroyOriginalProfileNow(Profile* const profile);

  // Destroys an OffTheRecord profile immediately and removes it from all
  // pending destroyers.
  static void DestroyOffTheRecordProfileNow(Profile* const profile);

  // Reset pending destroyers whose target profile matches the given one
  // to make it stop attempting to destroy it. Returns true if any object
  // object was found to match and get reset.
  static bool ResetPendingDestroyers(Profile* const profile);

  // We need access to all pending destroyers so we can cancel them.
  static DestroyerSet* pending_destroyers_;

  // We don't want to wait forever, so we have a cancellation timer.
  base::OneShotTimer timer_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      observations_{this};

  // The profile being destroyed. If it is set to NULL, it is a signal from
  // another instance of ProfileDestroyer that this instance is canceled.
  Profile* profile_;

  base::WeakPtrFactory<ProfileDestroyer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
