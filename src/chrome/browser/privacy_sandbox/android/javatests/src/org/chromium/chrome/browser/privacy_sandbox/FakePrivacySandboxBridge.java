// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * Java implementation of PrivacySandboxBridge for testing.
 */
class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsPrivacySandboxEnabled = true;

    private final HashMap<String, Topic> mTopics = new HashMap<>();
    private final Set<Topic> mCurrentTopTopics = new HashSet<>();
    private final Set<Topic> mBlockedTopics = new HashSet<>();
    private @DialogType int mDialogType = DialogType.NONE;
    private Integer mLastDialogAction;

    FakePrivacySandboxBridge() {
        setCurrentTopTopics("Foo", "Bar");
        setBlockedTopics("BlockedFoo", "BlockedBar");
    }

    public void setCurrentTopTopics(String... topics) {
        mCurrentTopTopics.clear();
        for (String name : topics) {
            mCurrentTopTopics.add(getOrCreateTopic(name));
        }
    }

    public void setBlockedTopics(String... topics) {
        mBlockedTopics.clear();
        for (String name : topics) {
            mBlockedTopics.add(getOrCreateTopic(name));
        }
    }

    private Topic getOrCreateTopic(String name) {
        Topic t = mTopics.get(name);
        if (t == null) {
            t = new Topic(mTopics.size(), -1, name);
            mTopics.put(name, t);
        }
        return t;
    }

    @Override
    public boolean isPrivacySandboxEnabled() {
        return mIsPrivacySandboxEnabled;
    }

    @Override
    public boolean isPrivacySandboxManaged() {
        return false;
    }

    @Override
    public void setPrivacySandboxEnabled(boolean enabled) {
        mIsPrivacySandboxEnabled = enabled;
    }

    @Override
    public boolean isFlocEnabled() {
        return true;
    }

    @Override
    public void setFlocEnabled(boolean enabled) {
        assert false;
    }

    @Override
    public boolean isFlocIdResettable() {
        return true;
    }

    @Override
    public void resetFlocId() {
        assert false;
    }

    @Override
    public String getFlocStatusString() {
        return null;
    }

    @Override
    public String getFlocGroupString() {
        return null;
    }

    @Override
    public String getFlocUpdateString() {
        return null;
    }

    @Override
    public String getFlocDescriptionString() {
        return null;
    }

    @Override
    public String getFlocResetExplanationString() {
        return null;
    }

    @Override
    public Topic[] getCurrentTopTopics() {
        return mCurrentTopTopics.toArray(new Topic[] {});
    }

    @Override
    public Topic[] getBlockedTopics() {
        return mBlockedTopics.toArray(new Topic[] {});
    }

    @Override
    public void setTopicAllowed(int topicId, int taxonomyVersion, boolean allowed) {
        Topic topic = null;
        for (Topic t : mTopics.values()) {
            if (t.getTopicId() == topicId) {
                topic = t;
            }
        }
        if (allowed) {
            mCurrentTopTopics.add(topic);
            mBlockedTopics.remove(topic);
        } else {
            mCurrentTopTopics.remove(topic);
            mBlockedTopics.add(topic);
        }
    }

    public void setRequiredDialogType(@DialogType int type) {
        mDialogType = type;
    }

    @Override
    public int getRequiredDialogType() {
        return mDialogType;
    }

    @Override
    public void dialogActionOccurred(@DialogAction int action) {
        mLastDialogAction = action;
    }

    public Integer getLastDialogAction() {
        return mLastDialogAction;
    }

    public void resetLastDialogAction() {
        mLastDialogAction = null;
    }
}
