// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.carousel;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * A chip to display to the user.
 */
@JNINamespace("autofill_assistant")
public class AssistantChip {
    @IntDef({Type.CHIP_ASSISTIVE, Type.BUTTON_FILLED_BLUE, Type.BUTTON_HAIRLINE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int CHIP_ASSISTIVE = 0;
        int BUTTON_FILLED_BLUE = 1;
        int BUTTON_HAIRLINE = 2;

        /** The number of types. Increment this value if you add a type. */
        int NUM_ENTRIES = 3;
    }

    /**
     * An icon that should be displayed next to the text. This is the java version of the ChipIcon
     * enum in //components/autofill_assistant/browser/model.proto. DO NOT change this without
     * adapting that proto enum.
     */
    @IntDef({Icon.NONE, Icon.CLEAR, Icon.DONE, Icon.REFRESH, Icon.OVERFLOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Icon {
        int NONE = 0;

        // https://icons.googleplex.com/#icon=clear
        int CLEAR = 1;

        // https://icons.googleplex.com/#icon=done
        int DONE = 2;

        // https://icons.googleplex.com/#icon=refresh
        int REFRESH = 3;

        // https://icons.googleplex.com/#icon=more_vert
        int OVERFLOW = 4;
    }

    /**
     * The type of the chip. This will impact the background, border and text colors of the chip.
     */
    private final @Type int mType;

    /** The icon, shown next to the text.*/
    private final @Icon int mIcon;

    /** The text displayed on the chip. */
    private final String mText;

    /** Whether this chip is enabled or not. */
    private boolean mDisabled;

    /** Whether this chip is visible or not. */
    private boolean mVisible;

    /**
     * Whether this chip is sticky. A sticky chip will be a candidate to be displayed in the header
     * if the peek mode of the sheet is HANDLE_HEADER.
     */
    private final boolean mSticky;

    /** The callback that will be triggered when this chip is clicked. */
    private Runnable mSelectedListener;

    /**
     * The list of popup items to show when the chip is tapped. When specified, the regular {@code
     * mSelectedListener} will be automatically replaced with a callback to display the popup menu.
     */
    private @Nullable List<String> mPopupItems;

    /** The callback to invoke when the n'th item in {@code mPopupItems} is selected. */
    private @Nullable Callback<Integer> mOnPopupItemSelected;

    /** The content description for the chip. */
    private final @Nullable String mContentDescription;

    /**
     * Optional identifier used to distinguish two otherwise equal buttons (like close and cancel).
     * It's only necessary when a chip in a container needs to update its listener without changing
     * any other property or removing it first.
     */
    private final String mOptionalIdentifier;

    public AssistantChip(@Type int type, @Icon int icon, String text, boolean disabled,
            boolean sticky, boolean visible, @Nullable String contentDescription,
            String optionalIdentifier) {
        mType = type;
        mIcon = icon;
        mText = text;
        mDisabled = disabled;
        mSticky = sticky;
        mVisible = visible;
        mOptionalIdentifier = optionalIdentifier;
        mContentDescription = contentDescription;
    }

    public AssistantChip(@Type int type, @Icon int icon, String text, boolean disabled,
            boolean sticky, boolean visible, @Nullable String contentDescription) {
        this(type, icon, text, disabled, sticky, visible, contentDescription, "");
    }

    public AssistantChip(@Type int type, @Icon int icon, String text, boolean disabled,
            boolean sticky, boolean visible, Runnable selectedListener,
            @Nullable String contentDescription) {
        this(type, icon, text, disabled, sticky, visible, contentDescription);
        assert selectedListener != null;
        mSelectedListener = selectedListener;
    }

    public int getType() {
        return mType;
    }

    public int getIcon() {
        return mIcon;
    }

    public String getText() {
        return mText;
    }

    public boolean isDisabled() {
        return mDisabled;
    }

    public boolean isVisible() {
        return mVisible;
    }

    public void setDisabled(boolean disabled) {
        mDisabled = disabled;
    }

    public void setVisible(boolean visible) {
        mVisible = visible;
    }

    public boolean isSticky() {
        return mSticky;
    }

    public @Nullable Runnable getSelectedListener() {
        return mSelectedListener;
    }

    public void setSelectedListener(Runnable selectedListener) {
        mSelectedListener = selectedListener;
    }

    public @Nullable String getContentDescription() {
        return mContentDescription;
    }

    public void setPopupItems(List<String> popupItems, Callback<Integer> onSelectedCallback) {
        mPopupItems = popupItems;
        mOnPopupItemSelected = onSelectedCallback;
    }

    public @Nullable List<String> getPopupItems() {
        return mPopupItems;
    }

    public @Nullable Callback<Integer> getOnPopupItemSelectedCallback() {
        return mOnPopupItemSelected;
    }

    public String getOptionalIdentifier() {
        return mOptionalIdentifier;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof AssistantChip)) {
            return false;
        }

        AssistantChip that = (AssistantChip) other;
        return this.getType() == that.getType() && this.getText().equals(that.getText())
                && this.getIcon() == that.getIcon() && this.isSticky() == that.isSticky()
                && this.isDisabled() == that.isDisabled() && this.isVisible() == that.isVisible()
                && this.getOptionalIdentifier().equals(that.getOptionalIdentifier());
    }

    /**
     * Creates a hairline assistant chip with an empty callback. The callback needs to be bound
     * before the view is inflated.
     */
    @CalledByNative
    public static AssistantChip createHairlineAssistantChip(int icon, String text, boolean disabled,
            boolean sticky, boolean visible, @Nullable String contentDescription,
            String optionalIdentifier) {
        return new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, icon, text, disabled, sticky,
                visible, contentDescription, optionalIdentifier);
    }

    /**
     * Creates a blue-filled assistant chip with an empty callback. The callback needs to be bound
     * before the view is inflated.
     */
    @CalledByNative
    public static AssistantChip createHighlightedAssistantChip(int icon, String text,
            boolean disabled, boolean sticky, boolean visible, @Nullable String contentDescription,
            String optionalIdentifier) {
        return new AssistantChip(Type.BUTTON_FILLED_BLUE, icon, text, disabled, sticky, visible,
                contentDescription, optionalIdentifier);
    }

    @CalledByNative
    private static List<AssistantChip> createChipList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addChipToList(List<AssistantChip> list, AssistantChip chip) {
        list.add(chip);
    }
}
