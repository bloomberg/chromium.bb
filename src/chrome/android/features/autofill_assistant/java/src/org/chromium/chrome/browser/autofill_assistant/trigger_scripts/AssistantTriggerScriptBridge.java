// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.AssistantCoordinator;
import org.chromium.chrome.browser.autofill_assistant.AssistantDependencies;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiController;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.List;

/**
 * Communicates with the native {@code TriggerScriptBridgeAndroid} to show and hide trigger
 * scripts.
 */
@JNINamespace("autofill_assistant")
public class AssistantTriggerScriptBridge {
    private final AssistantDependencies mDependencies;

    private final AssistantTriggerScript mTriggerScript;
    private long mNativeBridge;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;

    @CalledByNative
    public AssistantTriggerScriptBridge(AssistantDependencies dependencies) {
        mDependencies = dependencies;

        AssistantTriggerScript.Delegate delegate =
                new AssistantTriggerScript.Delegate() {
                    @Override
                    public void onTriggerScriptAction(int action) {
                        safeNativeOnTriggerScriptAction(action);
                    }

                    @Override
                    public void onBottomSheetClosedWithSwipe() {
                        safeNativeOnBottomSheetClosedWithSwipe();
                    }

                    @Override
                    public boolean onBackButtonPressed() {
                        return safeNativeOnBackButtonPressed();
                    }

                    @Override
                    public void onFeedbackButtonClicked() {
                        HelpAndFeedbackLauncherImpl.getInstance().showFeedback(
                                TabUtils.getActivity(
                                        TabUtils.fromWebContents(mDependencies.getWebContents())),
                                AutofillAssistantUiController.getProfile(),
                                mDependencies.getWebContents().getVisibleUrl().getSpec(),
                                AssistantCoordinator.FEEDBACK_CATEGORY_TAG);
                    }
                };

        mTriggerScript = new AssistantTriggerScript(dependencies.getContext(), delegate,
                dependencies.getWebContents(), dependencies.getBottomSheetController(),
                dependencies.getBottomInsetProvider(), dependencies.getAccessibilityUtil());

        mKeyboardVisibilityListener = this::safeNativeOnKeyboardVisibilityChanged;
    }

    /**
     * Re-creates the header and returns the new header model. Must be called before every
     * invocation of {@code showTriggerScript}. It is not possible to persist headers across
     * multiple shown trigger scripts.
     */
    @CalledByNative
    private AssistantHeaderModel createHeaderAndGetModel() {
        return mTriggerScript.createHeaderAndGetModel();
    }

    @CalledByNative
    private Context getContext() {
        return mDependencies.getContext();
    }

    /**
     * Used by native to update and show the UI. The header should be created and updated using
     * {@code createHeaderAndGetModel} prior to calling this function.
     * @return true if the trigger script was displayed, else false.
     */
    @CalledByNative
    private boolean showTriggerScript(String[] cancelPopupMenuItems, int[] cancelPopupMenuActions,
            List<AssistantChip> leftAlignedChips, int[] leftAlignedChipsActions,
            List<AssistantChip> rightAlignedChips, int[] rightAlignedChipsActions,
            boolean resizeVisualViewport, boolean scrollToHide) {
        // Trigger scripts currently do not support switching activities (such as CCT->tab).
        // TODO(b/171776026): Re-inject dependencies on activity change to support CCT->tab.
        if (TabUtils.getActivity(TabUtils.fromWebContents(mDependencies.getWebContents()))
                != mDependencies.getContext()) {
            return false;
        }

        // NOTE: the cancel popup menu must be set before the chips are bound.
        mTriggerScript.setCancelPopupMenu(cancelPopupMenuItems, cancelPopupMenuActions);
        mTriggerScript.setLeftAlignedChips(leftAlignedChips, leftAlignedChipsActions);
        mTriggerScript.setRightAlignedChips(rightAlignedChips, rightAlignedChipsActions);
        boolean shown = mTriggerScript.show(resizeVisualViewport, scrollToHide);

        // Track keyboard visibility while a trigger script is being shown.
        if (shown) {
            mDependencies.getKeyboardVisibilityDelegate().removeKeyboardVisibilityListener(
                    mKeyboardVisibilityListener);
            mDependencies.getKeyboardVisibilityDelegate().addKeyboardVisibilityListener(
                    mKeyboardVisibilityListener);
        }
        return shown;
    }

    @CalledByNative
    private void hideTriggerScript() {
        mTriggerScript.hide();
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        mNativeBridge = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeBridge = 0;
        mTriggerScript.destroy();
        mDependencies.getKeyboardVisibilityDelegate().removeKeyboardVisibilityListener(
                mKeyboardVisibilityListener);
    }

    private void safeNativeOnTriggerScriptAction(int action) {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onTriggerScriptAction(
                    mNativeBridge, AssistantTriggerScriptBridge.this, action);
        }
    }

    private void safeNativeOnBottomSheetClosedWithSwipe() {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onBottomSheetClosedWithSwipe(
                    mNativeBridge, AssistantTriggerScriptBridge.this);
        }
    }

    private boolean safeNativeOnBackButtonPressed() {
        if (mNativeBridge != 0) {
            return AssistantTriggerScriptBridgeJni.get().onBackButtonPressed(
                    mNativeBridge, AssistantTriggerScriptBridge.this);
        }
        return false;
    }

    private void safeNativeOnKeyboardVisibilityChanged(boolean visible) {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onKeyboardVisibilityChanged(
                    mNativeBridge, AssistantTriggerScriptBridge.this, visible);
        }
    }

    @NativeMethods
    interface Natives {
        void onTriggerScriptAction(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, int action);
        void onBottomSheetClosedWithSwipe(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
        boolean onBackButtonPressed(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
        void onKeyboardVisibilityChanged(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, boolean visible);
    }
}
