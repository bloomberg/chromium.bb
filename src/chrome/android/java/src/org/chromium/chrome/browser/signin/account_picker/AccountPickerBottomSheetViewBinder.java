// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless AccountPickerBottomSheet view binder.
 */
class AccountPickerBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AccountPickerBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.getSelectedAccountView().setOnClickListener(
                    model.get(AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED));
        } else if (propertyKey == AccountPickerBottomSheetProperties.VIEW_STATE) {
            @ViewState
            int viewState = model.get(AccountPickerBottomSheetProperties.VIEW_STATE);
            switchToState(view, viewState);
            if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST) {
                // This is needed when the view state changes from SIGNIN_AUTH_ERROR to
                // COLLAPSED_ACCOUNT_LIST
                view.updateSelectedAccount(
                        model.get(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA));
            }
        } else if (propertyKey == AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA) {
            DisplayableProfileData profileData =
                    model.get(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA);
            if (profileData != null) {
                view.updateSelectedAccount(profileData);
            }
        } else if (propertyKey == AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED) {
            view.getContinueAsButton().setOnClickListener(
                    model.get(AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED));
        } else if (propertyKey == AccountPickerBottomSheetProperties.ON_DISMISS_CLICKED) {
            view.getDismissButton().setOnClickListener(
                    model.get(AccountPickerBottomSheetProperties.ON_DISMISS_CLICKED));
        }
    }

    /**
     * Sets up the configuration of account picker bottom sheet according to the given
     * {@link ViewState}.
     */
    private static void switchToState(AccountPickerBottomSheetView view, @ViewState int viewState) {
        view.setTitleAndContentDescriptionStrings(getTitleId(viewState), getSubtitleId(viewState));

        switch (viewState) {
            case ViewState.NO_ACCOUNTS:
                view.collapseToNoAccountView();
                break;
            case ViewState.COLLAPSED_ACCOUNT_LIST:
                view.collapseAccountList();
                break;
            case ViewState.EXPANDED_ACCOUNT_LIST:
                view.expandAccountList();
                break;
            case ViewState.SIGNIN_IN_PROGRESS:
                view.setUpSignInInProgressView();
                break;
            case ViewState.INCOGNITO_INTERSTITIAL:
                view.setUpIncognitoInterstitialView();
                break;
            case ViewState.SIGNIN_GENERAL_ERROR:
                view.setUpSignInGeneralErrorView();
                break;
            case ViewState.SIGNIN_AUTH_ERROR:
                view.setUpSignInAuthErrorView();
                break;
            default:
                throw new IllegalArgumentException(
                        "Cannot bind AccountPickerBottomSheetView for the view state:" + viewState);
        }
        view.setAccessibilityFocusOnTitle();
    }

    private static @StringRes int getTitleId(@ViewState int viewState) {
        switch (viewState) {
            case ViewState.NO_ACCOUNTS:
            case ViewState.COLLAPSED_ACCOUNT_LIST:
            case ViewState.EXPANDED_ACCOUNT_LIST:
                return R.string.signin_account_picker_dialog_title;
            case ViewState.SIGNIN_IN_PROGRESS:
                return R.string.signin_account_picker_bottom_sheet_signin_title;
            case ViewState.INCOGNITO_INTERSTITIAL:
                return R.string.incognito_interstitial_title;
            case ViewState.SIGNIN_GENERAL_ERROR:
            case ViewState.SIGNIN_AUTH_ERROR:
                return R.string.signin_account_picker_bottom_sheet_error_title;
            default:
                throw new IllegalArgumentException("Unknown ViewState:" + viewState);
        }
    }

    private static @Nullable @StringRes Integer getSubtitleId(@ViewState int viewState) {
        switch (viewState) {
            case ViewState.NO_ACCOUNTS:
            case ViewState.COLLAPSED_ACCOUNT_LIST:
            case ViewState.EXPANDED_ACCOUNT_LIST:
                return R.string.signin_account_picker_bottom_sheet_subtitle;
            case ViewState.INCOGNITO_INTERSTITIAL:
                return R.string.incognito_interstitial_message;
            case ViewState.SIGNIN_GENERAL_ERROR:
                return R.string.signin_account_picker_general_error_subtitle;
            case ViewState.SIGNIN_AUTH_ERROR:
                return R.string.signin_account_picker_auth_error_subtitle;
            case ViewState.SIGNIN_IN_PROGRESS:
                return null;
            default:
                throw new IllegalArgumentException("Unknown ViewState:" + viewState);
        }
    }

    private AccountPickerBottomSheetViewBinder() {}
}
