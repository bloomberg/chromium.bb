// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import android.app.Activity;
import android.content.DialogInterface;
import android.view.View;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.widget.PromoDialog;
import org.chromium.ui.widget.Toast;

/**
 * The promo screen encouraging users to enable Data Saver.
 */
public class DataReductionPromoScreen extends PromoDialog {
    private static final String FORCE_PROMO_SWITCH = "force-data-reduction-second-run-promo";

    private int mState;

    private static boolean shouldLaunchDataReductionPromo(
            Activity parentActivity, boolean isIncognito) {
        // This switch is only used for testing so it is ok to override all the other checking.
        if (CommandLine.getInstance().hasSwitch(FORCE_PROMO_SWITCH)) return true;

        // The promo is displayed if Chrome is launched directly (i.e., not with the intent to
        // navigate to and view a URL on startup), the instance is part of the field trial,
        // and the promo has not been displayed before.
        if (isIncognito) return false;
        if (!DataReductionPromoUtils.canShowPromos()) return false;
        if (DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo()) return false;

        return true;
    }

    /**
     * Launch the data reduction promo, if it needs to be displayed.
     * @return Whether the data reduction promo was displayed.
     */
    public static boolean launchDataReductionPromo(Activity parentActivity, boolean isIncognito) {
        if (!shouldLaunchDataReductionPromo(parentActivity, isIncognito)) return false;

        DataReductionPromoScreen promoScreen = new DataReductionPromoScreen(parentActivity);
        promoScreen.setOnDismissListener(promoScreen);
        promoScreen.show();

        return true;
    }

    /**
     * DataReductionPromoScreen constructor.
     *
     * @param activity An Android activity to display the dialog.
     */
    public DataReductionPromoScreen(Activity activity) {
        super(activity);
        mState = DataReductionProxyUma.ACTION_DISMISSED;
    }

    @Override
    protected DialogParams getDialogParams() {
        PromoDialog.DialogParams params = new PromoDialog.DialogParams();
        params.vectorDrawableResource = R.drawable.data_reduction_illustration;
        params.headerStringResource =
                DataReductionBrandingResourceProvider.getDataSaverBrandedString(
                        R.string.data_reduction_promo_title);
        params.subheaderStringResource =
                DataReductionBrandingResourceProvider.getDataSaverBrandedString(
                        R.string.data_reduction_promo_summary);
        params.primaryButtonStringResource =
                DataReductionBrandingResourceProvider.getDataSaverBrandedString(
                        R.string.data_reduction_enable_button);
        params.secondaryButtonStringResource = R.string.no_thanks;
        return params;
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();

        if (id == R.id.button_secondary) {
            dismiss();
        } else if (id == R.id.button_primary) {
            mState = DataReductionProxyUma.ACTION_ENABLED;
            handleEnableButtonPressed();
        } else {
            assert false : "Unhandled onClick event";
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
    }

    private void handleEnableButtonPressed() {
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(getContext(), true);
        dismiss();
        Toast.makeText(getContext(),
                     getContext().getString(
                             DataReductionBrandingResourceProvider.getDataSaverBrandedString(
                                     R.string.data_reduction_enabled_toast)),
                     Toast.LENGTH_LONG)
                .show();
    }

    @Override
    public void dismiss() {
        if (mState < DataReductionProxyUma.ACTION_INDEX_BOUNDARY) {
            DataReductionProxyUma.dataReductionProxyUIAction(mState);
            mState = DataReductionProxyUma.ACTION_INDEX_BOUNDARY;
        }
        super.dismiss();
    }
}
