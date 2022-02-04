// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils.OfflinePageLoadUrlDelegate;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.paint_preview.TabbedPaintPreview;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoFeatures;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;

/**
 * Chrome's customization of PageInfoControllerDelegate. This class provides Chrome-specific info to
 * PageInfoController. It also contains logic for Chrome-specific features, like {@link
 * TabbedPaintPreview}
 */
public class ChromePageInfoControllerDelegate extends PageInfoControllerDelegate {
    private final WebContents mWebContents;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Context mContext;
    private final Profile mProfile;
    private final Supplier<StoreInfoActionHandler> mStoreInfoActionHandlerSupplier;
    private final ChromePageInfoHighlight mPageInfoHighlight;
    private String mOfflinePageCreationDate;
    private OfflinePageLoadUrlDelegate mOfflinePageLoadUrlDelegate;

    public ChromePageInfoControllerDelegate(Context context, WebContents webContents,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            OfflinePageLoadUrlDelegate offlinePageLoadUrlDelegate,
            @Nullable Supplier<StoreInfoActionHandler> storeInfoActionHandlerSupplier,
            ChromePageInfoHighlight pageInfoHighlight) {
        super(new ChromeAutocompleteSchemeClassifier(Profile.fromWebContents(webContents)),
                VrModuleProvider.getDelegate(),
                /** isSiteSettingsAvailable= */
                SiteSettingsHelper.isSiteSettingsAvailable(webContents),
                /** cookieControlsShown= */
                CookieControlsBridge.isCookieControlsEnabled(Profile.fromWebContents(webContents)));
        mContext = context;
        mWebContents = webContents;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mProfile = Profile.fromWebContents(mWebContents);
        mStoreInfoActionHandlerSupplier = storeInfoActionHandlerSupplier;
        mPageInfoHighlight = pageInfoHighlight;

        initOfflinePageParams();
        mOfflinePageLoadUrlDelegate = offlinePageLoadUrlDelegate;

        TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                .notifyEvent(EventConstants.PAGE_INFO_OPENED);
    }

    private void initOfflinePageParams() {
        mOfflinePageState = OfflinePageState.NOT_OFFLINE_PAGE;
        OfflinePageItem offlinePage = OfflinePageUtils.getOfflinePage(mWebContents);
        if (offlinePage != null) {
            mOfflinePageUrl = offlinePage.getUrl();
            if (OfflinePageUtils.isShowingTrustedOfflinePage(mWebContents)) {
                mOfflinePageState = OfflinePageState.TRUSTED_OFFLINE_PAGE;
            } else {
                mOfflinePageState = OfflinePageState.UNTRUSTED_OFFLINE_PAGE;
            }
            // Get formatted creation date of the offline page. If the page was shared (so the
            // creation date cannot be acquired), make date an empty string and there will be
            // specific processing for showing different string in UI.
            long pageCreationTimeMs = offlinePage.getCreationTimeMs();
            if (pageCreationTimeMs != 0) {
                Date creationDate = new Date(offlinePage.getCreationTimeMs());
                DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
                mOfflinePageCreationDate = df.format(creationDate);
            }
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManagerSupplier.get();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isInstantAppAvailable(String url) {
        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
        return instantAppsHandler.isInstantAppAvailable(
                url, false /* checkHoldback */, false /* includeUserPrefersBrowser */);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Intent getInstantAppIntentForUrl(String url) {
        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
        return instantAppsHandler.getInstantAppIntentForUrl(url);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void initOfflinePageUiParams(
            PageInfoView.Params viewParams, Consumer<Runnable> runAfterDismiss) {
        if (isShowingOfflinePage() && OfflinePageUtils.isConnected()) {
            viewParams.openOnlineButtonClickCallback = () -> {
                runAfterDismiss.accept(() -> {
                    // Attempt to reload to an online version of the viewed offline web page.
                    // This attempt might fail if the user is offline, in which case an offline
                    // copy will be reloaded.
                    OfflinePageUtils.reload(mWebContents, mOfflinePageLoadUrlDelegate);
                });
            };
        } else {
            viewParams.openOnlineButtonShown = false;
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @Nullable
    public String getOfflinePageConnectionMessage() {
        if (mOfflinePageState == OfflinePageState.TRUSTED_OFFLINE_PAGE) {
            return String.format(mContext.getString(R.string.page_info_connection_offline),
                    mOfflinePageCreationDate);
        } else if (mOfflinePageState == OfflinePageState.UNTRUSTED_OFFLINE_PAGE) {
            // For untrusted pages, if there's a creation date, show it in the message.
            if (TextUtils.isEmpty(mOfflinePageCreationDate)) {
                return mContext.getString(R.string.page_info_offline_page_not_trusted_without_date);
            } else {
                return String.format(
                        mContext.getString(R.string.page_info_offline_page_not_trusted_with_date),
                        mOfflinePageCreationDate);
            }
        }
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isShowingPaintPreviewPage() {
        Tab tab = TabUtils.fromWebContents(mWebContents);
        return tab != null && TabbedPaintPreview.get(tab).isShowing();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @Nullable
    public String getPaintPreviewPageConnectionMessage() {
        if (!isShowingPaintPreviewPage()) return null;

        return mContext.getString(R.string.page_info_connection_paint_preview);
    }

    @Override
    public void showCookieSettings() {
        SiteSettingsHelper.showCategorySettings(mContext, SiteSettingsCategory.Type.COOKIES);
    }

    @Override
    public Collection<PageInfoSubpageController> createAdditionalRowViews(
            PageInfoMainController mainController, ViewGroup rowWrapper) {
        Collection<PageInfoSubpageController> controllers = new ArrayList<>();
        if (PageInfoFeatures.PAGE_INFO_HISTORY.isEnabled()) {
            final Tab tab = TabUtils.fromWebContents(mWebContents);
            final PageInfoRowView historyRow = new PageInfoRowView(rowWrapper.getContext(), null);
            historyRow.setId(PageInfoHistoryController.HISTORY_ROW_ID);
            rowWrapper.addView(historyRow);
            controllers.add(new PageInfoHistoryController(
                    mainController, historyRow, this, () -> { return tab; }));
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PAGE_INFO_ABOUT_THIS_SITE)) {
            final PageInfoRowView aboutThisSiteRow =
                    new PageInfoRowView(rowWrapper.getContext(), null);
            aboutThisSiteRow.setId(PageInfoAboutThisSiteController.ROW_ID);
            rowWrapper.addView(aboutThisSiteRow);
            controllers.add(new PageInfoAboutThisSiteController(
                    mainController, aboutThisSiteRow, this, mWebContents));
        }
        if (PageInfoFeatures.PAGE_INFO_STORE_INFO.isEnabled() && !isIncognito()) {
            final PageInfoRowView storeInfoRow = new PageInfoRowView(rowWrapper.getContext(), null);
            storeInfoRow.setId(PageInfoStoreInfoController.STORE_INFO_ROW_ID);
            rowWrapper.addView(storeInfoRow);
            controllers.add(new PageInfoStoreInfoController(mainController, storeInfoRow,
                    mStoreInfoActionHandlerSupplier,
                    mPageInfoHighlight.shouldHighlightStoreInfo()));
        }
        return controllers;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public CookieControlsBridge createCookieControlsBridge(CookieControlsObserver observer) {
        return new CookieControlsBridge(observer, mWebContents,
                mProfile.isOffTheRecord() ? mProfile.getOriginalProfile() : null);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public BrowserContextHandle getBrowserContext() {
        return mProfile;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public SiteSettingsDelegate getSiteSettingsDelegate() {
        return new ChromeSiteSettingsDelegate(mContext, mProfile);
    }

    @NonNull
    @Override
    public void getFavicon(GURL url, Callback<Drawable> callback) {
        Resources resources = mContext.getResources();
        int size = resources.getDimensionPixelSize(R.dimen.page_info_favicon_size);
        new FaviconHelper().getLocalFaviconImageForURL(mProfile, url, size, (image, iconUrl) -> {
            if (image != null) {
                callback.onResult(new BitmapDrawable(resources, image));
            } else if (UrlUtilities.isInternalScheme(url)) {
                callback.onResult(
                        TintedDrawable.constructTintedDrawable(mContext, R.drawable.chromelogo16));
            } else {
                callback.onResult(null);
            }
        });
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled();
    }

    @VisibleForTesting
    void setOfflinePageStateForTesting(@OfflinePageState int offlinePageState) {
        mOfflinePageState = offlinePageState;
    }

    @Override
    public FragmentManager getFragmentManager() {
        FragmentActivity activity = ((FragmentActivity) mContext);
        if (activity.isFinishing()) return null;
        return activity.getSupportFragmentManager();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
    }
}
