// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils.OfflinePageLoadUrlDelegate;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.paint_preview.TabbedPaintPreview;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.previews.PreviewsUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsClient;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsClient;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.Date;

/**
 * Chrome's customization of PageInfoControllerDelegate. This class provides Chrome-specific info to
 * PageInfoController. It also contains logic for Chrome-specific features, like {@link Previews}
 */
public class ChromePageInfoControllerDelegate extends PageInfoControllerDelegate {
    private final WebContents mWebContents;
    private final Context mContext;
    private final Profile mProfile;
    private String mOfflinePageCreationDate;
    private OfflinePageLoadUrlDelegate mOfflinePageLoadUrlDelegate;

    public ChromePageInfoControllerDelegate(Context context, WebContents webContents,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            OfflinePageLoadUrlDelegate offlinePageLoadUrlDelegate) {
        super(modalDialogManagerSupplier,
                new ChromeAutocompleteSchemeClassifier(Profile.fromWebContents(webContents)),
                VrModuleProvider.getDelegate(),
                /** isSiteSettingsAvailable= */
                SiteSettingsHelper.isSiteSettingsAvailable(webContents),
                /** cookieControlsShown= */
                CookieControlsBridge.isCookieControlsEnabled(Profile.fromWebContents(webContents)));
        mContext = context;
        mWebContents = webContents;
        mProfile = Profile.fromWebContents(mWebContents);

        mPreviewPageState = getPreviewPageStateAndRecordUma();
        initOfflinePageParams();
        mOfflinePageLoadUrlDelegate = offlinePageLoadUrlDelegate;
        initHttpsImageCompressionStateAndRecordUMA();
    }

    /**
     * Return the state of the webcontents showing the preview.
     */
    private @PreviewPageState int getPreviewPageStateAndRecordUma() {
        final int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
        @PreviewPageState
        int previewPageState = PreviewPageState.NOT_PREVIEW;
        final PreviewsAndroidBridge bridge = PreviewsAndroidBridge.getInstance();
        if (bridge.shouldShowPreviewUI(mWebContents)) {
            previewPageState = securityLevel == ConnectionSecurityLevel.SECURE
                    ? PreviewPageState.SECURE_PAGE_PREVIEW
                    : PreviewPageState.INSECURE_PAGE_PREVIEW;

            PreviewsUma.recordPageInfoOpened(bridge.getPreviewsType(mWebContents));
            TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                    EventConstants.PREVIEWS_VERBOSE_STATUS_OPENED);
        }
        return previewPageState;
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

    private void initHttpsImageCompressionStateAndRecordUMA() {
        mIsHttpsImageCompressionApplied =
                PreviewsAndroidBridge.getInstance().isHttpsImageCompressionApplied(mWebContents);
        if (mIsHttpsImageCompressionApplied) {
            PreviewsUma.recordHttpsImageCompressionPageInfoOpened();
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void initPreviewUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        final PreviewsAndroidBridge bridge = PreviewsAndroidBridge.getInstance();
        viewParams.previewSeparatorShown =
                mPreviewPageState == PreviewPageState.INSECURE_PAGE_PREVIEW;
        viewParams.previewUIShown = isShowingPreview();
        if (isShowingPreview()) {
            viewParams.urlTitleShown = false;
            viewParams.connectionMessageShown = false;

            viewParams.previewShowOriginalClickCallback = () -> {
                runAfterDismiss.accept(() -> {
                    PreviewsUma.recordOptOut(bridge.getPreviewsType(mWebContents));
                    bridge.loadOriginal(mWebContents);
                });
            };
            final String previewOriginalHost =
                    bridge.getOriginalHost(mWebContents.getVisibleUrlString());
            final String loadOriginalText = mContext.getString(
                    R.string.page_info_preview_load_original, previewOriginalHost);
            final SpannableString loadOriginalSpan = SpanApplier.applySpans(loadOriginalText,
                    new SpanInfo("<link>", "</link>",
                            // The callback given to NoUnderlineClickableSpan is overridden in
                            // PageInfoView so use previewShowOriginalClickCallback (above) instead
                            // because the entire TextView will be clickable.
                            new NoUnderlineClickableSpan(mContext.getResources(), (view) -> {})));
            viewParams.previewLoadOriginalMessage = loadOriginalSpan;
        }
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
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
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

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean shouldShowPerformanceBadge(String url) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PAGE_INFO_PERFORMANCE_HINTS)) {
            return false;
        }
        @PerformanceClass
        int pagePerformanceClass =
                PerformanceHintsObserver.getPerformanceClassForURL(mWebContents, url);
        return pagePerformanceClass == PerformanceClass.PERFORMANCE_FAST;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void showSiteSettings(String url) {
        SiteSettingsHelper.showSiteSettings(mContext, url);
    }

    @Override
    public void showCookieSettings() {
        SiteSettingsHelper.showCategorySettings(mContext, SiteSettingsCategory.Type.COOKIES);
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
    public SiteSettingsClient getSiteSettingsClient() {
        return new ChromeSiteSettingsClient(mContext, getBrowserContext());
    }

    @NonNull
    @Override
    public void getFavicon(String url, Callback<Drawable> callback) {
        Resources resources = mContext.getResources();
        int size = resources.getDimensionPixelSize(R.dimen.page_info_favicon_size);
        new FaviconHelper().getLocalFaviconImageForURL(mProfile, url, size, (image, iconUrl) -> {
            if (isShowingPreview()) {
                callback.onResult(SettingsUtils.getTintedIcon(mContext,
                        R.drawable.preview_pin_round, R.color.infobar_icon_drawable_color));
            } else if (image != null) {
                callback.onResult(new BitmapDrawable(resources, image));
            } else if (UrlUtilities.isInternalScheme(new GURL(url))) {
                callback.onResult(
                        AppCompatResources.getDrawable(mContext, R.drawable.chromelogo16));
            } else {
                callback.onResult(null);
            }
        });
    }

    @Override
    public Drawable getPreviewUiIcon() {
        return SettingsUtils.getTintedIcon(mContext,
            R.drawable.preview_pin_round, R.color.infobar_icon_drawable_color);
    }

    @VisibleForTesting
    void setOfflinePageStateForTesting(@OfflinePageState int offlinePageState) {
        mOfflinePageState = offlinePageState;
    }
}
