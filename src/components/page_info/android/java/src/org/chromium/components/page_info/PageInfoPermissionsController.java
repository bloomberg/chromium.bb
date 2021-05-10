// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.res.Resources;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoDiscoverabilityMetrics.DiscoverabilityAction;

import java.util.List;

/**
 * Class for controlling the page info permissions section.
 */
public class PageInfoPermissionsController
        implements PageInfoSubpageController, SingleWebsiteSettings.Observer {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final String mTitle;
    private final String mPageUrl;
    private SingleWebsiteSettings mSubPage;
    @ContentSettingsType
    private int mHighlightedPermission = ContentSettingsType.DEFAULT;
    @ColorRes
    private int mHighlightColor;
    private final PageInfoDiscoverabilityMetrics mDiscoverabilityMetrics =
            new PageInfoDiscoverabilityMetrics();

    public PageInfoPermissionsController(PageInfoMainController mainController,
            PageInfoRowView view, PageInfoControllerDelegate delegate, String pageUrl,
            @ContentSettingsType int highlightedPermission) {
        mMainController = mainController;
        mRowView = view;
        mDelegate = delegate;
        mPageUrl = pageUrl;
        mHighlightedPermission = highlightedPermission;
        Resources resources = mRowView.getContext().getResources();
        mHighlightColor = resources.getColor(R.color.iph_highlight_blue);
        mTitle = resources.getString(R.string.page_info_permissions_title);
    }

    private void launchSubpage() {
        if (mHighlightedPermission != ContentSettingsType.DEFAULT) {
            mDiscoverabilityMetrics.recordDiscoverabilityAction(
                    DiscoverabilityAction.PERMISSIONS_OPENED);
        }
        mMainController.recordAction(PageInfoAction.PAGE_INFO_PERMISSION_DIALOG_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mTitle;
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (fragmentManager.isStateSaved()) return null;

        Bundle fragmentArgs = SingleWebsiteSettings.createFragmentArgsForSite(mPageUrl);
        mSubPage = (SingleWebsiteSettings) Fragment.instantiate(
                mRowView.getContext(), SingleWebsiteSettings.class.getName(), fragmentArgs);
        mSubPage.setSiteSettingsDelegate(mDelegate.getSiteSettingsDelegate());
        mSubPage.setHideNonPermissionPreferences(true);
        mSubPage.setWebsiteSettingsObserver(this);
        if (mHighlightedPermission != ContentSettingsType.DEFAULT) {
            mSubPage.setHighlightedPermission(mHighlightedPermission, mHighlightColor);
        }
        fragmentManager.beginTransaction().add(mSubPage, null).commitNow();
        return mSubPage.requireView();
    }

    @Override
    public void onSubpageRemoved() {
        assert mSubPage != null;
        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        SingleWebsiteSettings subPage = mSubPage;
        mSubPage = null;
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (fragmentManager == null || fragmentManager.isStateSaved()) return;
        fragmentManager.beginTransaction().remove(subPage).commitNow();
    }

    public void setPermissions(PageInfoView.PermissionParams params) {
        Resources resources = mRowView.getContext().getResources();
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.ic_tune_24dp;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        rowParams.subtitle = getPermissionSummaryString(params.permissions, resources);
        rowParams.visible = mDelegate.isSiteSettingsAvailable() && rowParams.subtitle != null;
        if (mHighlightedPermission != ContentSettingsType.DEFAULT) {
            rowParams.rowTint = mHighlightColor;
        }
        mRowView.setParams(rowParams);
    }

    /**
     * Returns the most comprehensive subtitle summary string.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static String getPermissionSummaryString(
            List<PageInfoView.PermissionRowParams> permissions, Resources resources) {
        int numPermissions = permissions.size();
        if (numPermissions == 0) {
            return null;
        }

        PageInfoView.PermissionRowParams perm1 = permissions.get(0);
        boolean same = true;
        for (PageInfoView.PermissionRowParams perm : permissions) {
            if (perm.warningTextResource != 0) {
                // Show the first (most important warning) only, if there is one.
                return resources.getString(R.string.page_info_permissions_os_warning,
                        perm.name.toString(), resources.getString(perm.warningTextResource));
            }
            // Whether all permissions have had the same status so far.
            same = same && (perm1.allowed == perm.allowed);
        }

        if (numPermissions == 1) {
            int resId = perm1.allowed ? R.string.page_info_permissions_summary_1_allowed
                                      : R.string.page_info_permissions_summary_1_blocked;
            return resources.getString(resId, perm1.name.toString());
        }

        PageInfoView.PermissionRowParams perm2 = permissions.get(1);
        if (numPermissions == 2) {
            if (same) {
                int resId = perm1.allowed ? R.string.page_info_permissions_summary_2_allowed
                                          : R.string.page_info_permissions_summary_2_blocked;
                return resources.getString(resId, perm1.name.toString(), perm2.name.toString());
            }
            int resId = R.string.page_info_permissions_summary_2_mixed;
            // Put the allowed permission first.
            return resources.getString(resId,
                    perm1.allowed ? perm1.name.toString() : perm2.name.toString(),
                    perm1.allowed ? perm2.name.toString() : perm1.name.toString());
        }

        // More than 2 permissions.
        if (same) {
            int resId = perm1.allowed ? R.plurals.page_info_permissions_summary_more_allowed
                                      : R.plurals.page_info_permissions_summary_more_blocked;
            return resources.getQuantityString(resId, numPermissions - 2, perm1.name.toString(),
                    perm2.name.toString(), numPermissions - 2);
        }
        int resId = R.plurals.page_info_permissions_summary_more_mixed;
        return resources.getQuantityString(resId, numPermissions - 2, perm1.name.toString(),
                perm2.name.toString(), numPermissions - 2);
    }

    // SingleWebsiteSettings.Observer methods

    @Override
    public void onPermissionsReset() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_PERMISSIONS_CLEARED);
        mMainController.refreshPermissions();
        mMainController.exitSubpage();
    }

    @Override
    public void onPermissionChanged() {
        if (mHighlightedPermission != ContentSettingsType.DEFAULT) {
            mDiscoverabilityMetrics.recordDiscoverabilityAction(
                    DiscoverabilityAction.PERMISSION_CHANGED);
        }
        mMainController.recordAction(PageInfoAction.PAGE_INFO_CHANGED_PERMISSION);
        mMainController.refreshPermissions();
    }
}
