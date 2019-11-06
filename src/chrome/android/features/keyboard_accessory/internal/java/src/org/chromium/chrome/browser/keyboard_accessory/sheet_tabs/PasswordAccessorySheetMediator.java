// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
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
 * This class contains all logic for the password accessory sheet component. Changes to its internal
 * {@link PropertyModel} are observed by a {@link PropertyModelChangeProcessor} and affect the
 * password accessory sheet tab view.
 */
class PasswordAccessorySheetMediator implements Provider.Observer<AccessorySheetData> {
    private final AccessorySheetTabModel mModel;

    @Override
    public void onItemAvailable(int typeId, AccessorySheetData accessorySheetData) {
        mModel.set(splitIntoDataPieces(accessorySheetData));
    }

    PasswordAccessorySheetMediator(AccessorySheetTabModel model) {
        mModel = model;
    }

    void onTabShown() {
        ManualFillingMetricsRecorder.recordActionImpression(AccessoryAction.MANAGE_PASSWORDS);
        AccessorySheetTabMetricsRecorder.recordSheetSuggestions(AccessoryTabType.PASSWORDS, mModel);
    }

    private AccessorySheetDataPiece[] splitIntoDataPieces(AccessorySheetData accessorySheetData) {
        if (accessorySheetData == null) return new AccessorySheetDataPiece[0];

        List<AccessorySheetDataPiece> items = new ArrayList<>();
        if (shouldShowTitle(accessorySheetData.getUserInfoList())) {
            items.add(new AccessorySheetDataPiece(accessorySheetData.getTitle(), Type.TITLE));
        }
        for (UserInfo userInfo : accessorySheetData.getUserInfoList()) {
            items.add(new AccessorySheetDataPiece(userInfo, Type.PASSWORD_INFO));
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
