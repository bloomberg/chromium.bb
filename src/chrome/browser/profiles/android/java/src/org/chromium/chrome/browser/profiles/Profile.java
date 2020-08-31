// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/**
 * Wrapper that allows passing a Profile reference around in the Java layer.
 */
public class Profile implements BrowserContextHandle {
    /** Holds OTRProfileID for OffTheRecord profiles. Is null for regular profiles. */
    @Nullable
    private final OTRProfileID mOTRProfileID;

    /** Pointer to the Native-side ProfileAndroid. */
    private long mNativeProfileAndroid;

    private Profile(long nativeProfileAndroid) {
        mNativeProfileAndroid = nativeProfileAndroid;
        if (ProfileJni.get().isOffTheRecord(mNativeProfileAndroid, Profile.this)) {
            mOTRProfileID = ProfileJni.get().getOTRProfileID(mNativeProfileAndroid, Profile.this);
        } else {
            mOTRProfileID = null;
        }
    }

    /**
     * Returns the original profile, even if it is called in an incognito context.
     *
     * https://crbug.com/1041781: Remove after auditing and replacing all usecases.
     *
     * @deprecated use {@link #getLastUsedRegularProfile()} instead.
     */
    @Deprecated
    public static Profile getLastUsedProfile() {
        return getLastUsedRegularProfile();
    }

    /**
     * Returns the regular (i.e., not off-the-record) profile.
     *
     * Note: The function name uses the "last used" terminology for consistency with
     * profile_manager.cc which supports multiple regular profiles.
     */
    public static Profile getLastUsedRegularProfile() {
        // TODO(crbug.com/704025): turn this into an assert once the bug is fixed
        if (!ProfileManager.isInitialized()) {
            throw new IllegalStateException("Browser hasn't finished initialization yet!");
        }
        return (Profile) ProfileJni.get().getLastUsedRegularProfile();
    }

    /**
     * @param webContents {@link WebContents} object.
     * @return {@link Profile} object associated with the given WebContents.
     */
    public static Profile fromWebContents(WebContents webContents) {
        return (Profile) ProfileJni.get().fromWebContents(webContents);
    }

    /**
     * Destroys the Profile.  Destruction is delayed until all associated
     * renderers have been killed, so the profile might not be destroyed upon returning from
     * this call.
     */
    public void destroyWhenAppropriate() {
        ProfileJni.get().destroyWhenAppropriate(mNativeProfileAndroid, Profile.this);
    }

    public Profile getOriginalProfile() {
        return (Profile) ProfileJni.get().getOriginalProfile(mNativeProfileAndroid, Profile.this);
    }

    /**
     * Returns the primary OffTheRecord profile.
     *
     * @deprecated use {@link #getOffTheRecordProfile(OTRProfileID)} or {@link
     *         #getPrimaryOTRProfile()} instead.
     */
    @Deprecated
    public Profile getOffTheRecordProfile() {
        return getPrimaryOTRProfile();
    }

    /**
     * Returns the OffTheRecord profile with given OTRProfileiD.
     *
     * @param profileID {@link OTRProfileID} object.
     */
    public Profile getOffTheRecordProfile(OTRProfileID profileID) {
        assert profileID != null;
        return (Profile) ProfileJni.get().getOffTheRecordProfile(
                mNativeProfileAndroid, Profile.this, profileID);
    }

    /**
     * Returns the OffTheRecord profile for incognito tabs.
     */
    public Profile getPrimaryOTRProfile() {
        return (Profile) ProfileJni.get().getPrimaryOTRProfile(mNativeProfileAndroid, Profile.this);
    }

    /**
     * Returns the OffTheRecord profile id for OffTheRecord profiles, and null for regular profiles.
     */
    @Nullable
    public OTRProfileID getOTRProfileID() {
        return mOTRProfileID;
    }

    /**
     * Returns if primary OffTheRecord profile exists.
     *
     * @deprecated use {@link #hasOffTheRecordProfile(OTRProfileID)} or {@link
     *         #hasPrimaryOTRProfile()} instead.
     */
    @Deprecated
    public boolean hasOffTheRecordProfile() {
        return hasPrimaryOTRProfile();
    }

    /**
     * Returns if OffTheRecord profile with given OTRProfileID exists.
     *
     * @param profileID {@link OTRProfileID} object.
     */
    public boolean hasOffTheRecordProfile(OTRProfileID profileID) {
        assert profileID != null;
        return ProfileJni.get().hasOffTheRecordProfile(
                mNativeProfileAndroid, Profile.this, profileID);
    }

    /**
     * Returns if primary OffTheRecord profile exists.
     */
    public boolean hasPrimaryOTRProfile() {
        return ProfileJni.get().hasPrimaryOTRProfile(mNativeProfileAndroid, Profile.this);
    }

    public ProfileKey getProfileKey() {
        return (ProfileKey) ProfileJni.get().getProfileKey(mNativeProfileAndroid, Profile.this);
    }

    public boolean isOffTheRecord() {
        return mOTRProfileID != null;
    }

    /**
     * @return Whether the profile is signed in to a child account.
     */
    public boolean isChild() {
        return ProfileJni.get().isChild(mNativeProfileAndroid, Profile.this);
    }

    /**
     * Wipes all data for this profile.
     */
    public void wipe() {
        ProfileJni.get().wipe(mNativeProfileAndroid, Profile.this);
    }

    /**
     * @return Whether or not the native side profile exists.
     */
    @VisibleForTesting
    public boolean isNativeInitialized() {
        return mNativeProfileAndroid != 0;
    }

    @Override
    public long getNativeBrowserContextPointer() {
        return ProfileJni.get().getBrowserContextPointer(mNativeProfileAndroid);
    }

    @CalledByNative
    private static Profile create(long nativeProfileAndroid) {
        return new Profile(nativeProfileAndroid);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeProfileAndroid = 0;

        if (mOTRProfileID != null) {
            CookiesFetcher.deleteCookiesIfNecessary();
        }

        ProfileManager.onProfileDestroyed(this);
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeProfileAndroid;
    }

    @NativeMethods
    public interface Natives {
        Object getLastUsedRegularProfile();
        Object fromWebContents(WebContents webContents);
        void destroyWhenAppropriate(long nativeProfileAndroid, Profile caller);
        Object getOriginalProfile(long nativeProfileAndroid, Profile caller);
        Object getOffTheRecordProfile(
                long nativeProfileAndroid, Profile caller, OTRProfileID otrProfileID);
        Object getPrimaryOTRProfile(long nativeProfileAndroid, Profile caller);
        boolean hasOffTheRecordProfile(
                long nativeProfileAndroid, Profile caller, OTRProfileID otrProfileID);
        boolean hasPrimaryOTRProfile(long nativeProfileAndroid, Profile caller);
        boolean isOffTheRecord(long nativeProfileAndroid, Profile caller);
        boolean isChild(long nativeProfileAndroid, Profile caller);
        void wipe(long nativeProfileAndroid, Profile caller);
        Object getProfileKey(long nativeProfileAndroid, Profile caller);
        long getBrowserContextPointer(long nativeProfileAndroid);
        OTRProfileID getOTRProfileID(long nativeProfileAndroid, Profile caller);
    }
}
