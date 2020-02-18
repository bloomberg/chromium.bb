// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.support.annotation.CallSuper;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class contains the logic for the simple accessory sheets. Changes to its internal
 * {@link PropertyModel} are observed by a {@link PropertyModelChangeProcessor} and affect the
 * accessory sheet tab view.
 */
class AccessorySheetTabMediator implements Provider.Observer<AccessorySheetData> {
    private final AccessorySheetTabModel mModel;
    private final @AccessoryTabType int mTabType;
    private final @Type int mUserInfoType;

    @Override
    public void onItemAvailable(int typeId, AccessorySheetData accessorySheetData) {
        mModel.set(splitIntoDataPieces(accessorySheetData));
    }

    AccessorySheetTabMediator(
            AccessorySheetTabModel model, @AccessoryTabType int tabType, @Type int userInfoType) {
        mModel = model;
        mTabType = tabType;
        mUserInfoType = userInfoType;
    }

    @CallSuper
    void onTabShown() {
        AccessorySheetTabMetricsRecorder.recordSheetSuggestions(mTabType, mModel);
    }

    private AccessorySheetDataPiece[] splitIntoDataPieces(AccessorySheetData accessorySheetData) {
        if (accessorySheetData == null) return new AccessorySheetDataPiece[0];

        List<AccessorySheetDataPiece> items = new ArrayList<>();
        if (shouldShowTitle(accessorySheetData.getUserInfoList())) {
            items.add(new AccessorySheetDataPiece(accessorySheetData.getTitle(), Type.TITLE));
        }
        for (UserInfo userInfo : accessorySheetData.getUserInfoList()) {
            items.add(new AccessorySheetDataPiece(userInfo, mUserInfoType));
        }
        for (FooterCommand command : accessorySheetData.getFooterCommands()) {
            items.add(new AccessorySheetDataPiece(command, Type.FOOTER_COMMAND));
        }

        return items.toArray(new AccessorySheetDataPiece[0]);
    }

    private boolean shouldShowTitle(List<UserInfo> userInfoList) {
        return !ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
                || userInfoList.isEmpty();
    }
}
