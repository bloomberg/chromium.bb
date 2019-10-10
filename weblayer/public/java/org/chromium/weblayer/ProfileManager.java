package org.chromium.weblayer;

import org.chromium.weblayer_private.aidl.IProfile;

import java.util.HashMap;
import java.util.Map;

/* package */ class ProfileManager {

    private final Map<IProfile, Profile> mProfiles = new HashMap<>();

    /** Returns existing or new Profile associated with the given implementation on WebLayer side */
    Profile getProfileFor(IProfile impl) {
        Profile profile = mProfiles.get(impl);
        if (profile != null) {
            return profile;
        }

        profile = new Profile(impl, () -> mProfiles.remove(impl));
        mProfiles.put(impl, profile);
        return profile;
    }

    /** Destroys all the Profiles. */
    void destroy() {
        for (Profile profile : mProfiles.values()) {
            profile.destroy();
        }
        mProfiles.clear();
    }
}
