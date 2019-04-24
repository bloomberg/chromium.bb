// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JCaller;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to interface with send_tab_to_self_android_bridge which interacts with the corresponding
 * sync service. This is used by SendTabToSelfShareActivity when a user taps to share a tab. The
 * bridge is created and destroyed within the same method call.
 *
 * <p>TODO(tgupta): Add a paragraph describing the expected lifecycle of this class I.e. who would
 * own it, when is it destroyed, etc?
 */
@JNINamespace("send_tab_to_self")
public class SendTabToSelfAndroidBridge {
    // TODO(tgupta): Add logic back in to track whether model is loaded.
    private boolean mIsNativeSendTabToSelfModelLoaded;
    private long mNativeSendTabToSelfAndroidBridge;

    /**
     * Handler to fetch the share entries
     *
     * @param profile Profile instance corresponding to the active profile.
     */
    public SendTabToSelfAndroidBridge(Profile profile) {
        Natives jni = SendTabToSelfAndroidBridgeJni.get();
        mNativeSendTabToSelfAndroidBridge = jni.init(this, profile);
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        if (mNativeSendTabToSelfAndroidBridge != 0) {
            Natives jni = SendTabToSelfAndroidBridgeJni.get();
            jni.destroy(this, mNativeSendTabToSelfAndroidBridge);
            mNativeSendTabToSelfAndroidBridge = 0;
            mIsNativeSendTabToSelfModelLoaded = false;
        }
    }

    /** @returns All GUIDs for all SendTabToSelf entries */
    public List<String> getAllGuids() {
        // TODO(tgupta): Add this assertion back in once the code to load is in place.
        // assert mIsNativeSendTabToSelfModelLoaded;
        List<String> toPopulate = new ArrayList<String>();
        Natives jni = SendTabToSelfAndroidBridgeJni.get();
        jni.getAllGuids(this, mNativeSendTabToSelfAndroidBridge, toPopulate);
        return toPopulate;
    }

    /**
     * Called by the native code in order to populate the list.
     *
     * @param allGuids List to populate provided by getAllGuids
     * @param newGuid The GUID to add to the list
     */
    @CalledByNative
    private static void addToGuidList(List<String> allGuids, String newGuid) {
        allGuids.add(newGuid);
    }

    /** Deletes all SendTabToSelf entries. This is called when the user disables sync. */
    public void deleteAllEntries() {
        // TODO(tgupta): Add this assertion back in once the code to load is in place.
        // assert mIsNativeSendTabToSelfModelLoaded;
        Natives jni = SendTabToSelfAndroidBridgeJni.get();
        jni.deleteAllEntries(this, mNativeSendTabToSelfAndroidBridge);
    }

    /**
     * Creates a new entry to be persisted to the sync backend.
     *
     * @param url URL to be shared
     * @param title Title of the page
     * @return The persisted entry which contains additional information such as the GUID or null in
     *     the case of an error.
     */
    @Nullable
    public SendTabToSelfEntry addEntry(String url, String title) {
        // TODO(tgupta): Add this assertion back in once the code to load is in place.
        // assert mIsNativeSendTabToSelfModelLoaded;
        Natives jni = SendTabToSelfAndroidBridgeJni.get();
        return jni.addEntry(this, mNativeSendTabToSelfAndroidBridge, url, title);
    }

    /**
     * Return the entry associated with a particular GUID
     *
     * @param guid The GUID to retrieve the entry for
     * @return The found entry or null if none exists
     */
    @Nullable
    public SendTabToSelfEntry getEntryByGUID(String guid) {
        // TODO(tgupta): Add this assertion back in once the code to load is in place.
        // assert mIsNativeSendTabToSelfModelLoaded;
        Natives jni = SendTabToSelfAndroidBridgeJni.get();
        return jni.getEntryByGUID(this, mNativeSendTabToSelfAndroidBridge, guid);
    }

    @NativeMethods
    interface Natives {
        long init(@JCaller SendTabToSelfAndroidBridge bridge, Profile profile);

        void destroy(
                @JCaller SendTabToSelfAndroidBridge bridge, long nativeSendTabToSelfAndroidBridge);

        SendTabToSelfEntry addEntry(@JCaller SendTabToSelfAndroidBridge bridge,
                long nativeSendTabToSelfAndroidBridge, String url, String title);

        void getAllGuids(@JCaller SendTabToSelfAndroidBridge bridge,
                long nativeSendTabToSelfAndroidBridge, List<String> guids);

        void deleteAllEntries(
                @JCaller SendTabToSelfAndroidBridge bridge, long nativeSendTabToSelfAndroidBridge);

        SendTabToSelfEntry getEntryByGUID(@JCaller SendTabToSelfAndroidBridge bridge,
                long nativeSendTabToSelfAndroidBridge, String guid);
    }
}
