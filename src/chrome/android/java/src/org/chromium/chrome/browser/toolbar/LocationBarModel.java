// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.dom_distiller.DomDistillerServiceFactory;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * Provides a way of accessing toolbar data and state.
 */
public class LocationBarModel implements ToolbarDataProvider {
    private final Context mContext;

    private Tab mTab;
    private boolean mIsIncognito;
    private int mPrimaryColor;
    private boolean mIsUsingBrandColor;

    private long mNativeLocationBarModelAndroid;

    /**
     * Default constructor for this class.
     * @param context The Context used for styling the toolbar visuals.
     */
    public LocationBarModel(Context context) {
        mContext = context;
        mPrimaryColor = ColorUtils.getDefaultThemeColor(context.getResources(), false);
    }

    /**
     * Handle any initialization that must occur after native has been initialized.
     */
    public void initializeWithNative() {
        mNativeLocationBarModelAndroid = nativeInit();
    }

    /**
     * Destroys the native LocationBarModel.
     */
    public void destroy() {
        if (mNativeLocationBarModelAndroid == 0) return;
        nativeDestroy(mNativeLocationBarModelAndroid);
        mNativeLocationBarModelAndroid = 0;
    }

    /**
     * @return The currently active WebContents being used by the Toolbar.
     */
    @CalledByNative
    private WebContents getActiveWebContents() {
        if (!hasTab()) return null;
        return mTab.getWebContents();
    }

    /**
     * Sets the tab that contains the information to be displayed in the toolbar.
     * @param tab The tab associated currently with the toolbar.
     * @param isIncognito Whether the incognito model is currently selected, which must match the
     *                    passed in tab if non-null.
     */
    public void setTab(Tab tab, boolean isIncognito) {
        assert tab == null || tab.isIncognito() == isIncognito;
        mTab = tab;
        mIsIncognito = isIncognito;
        updateUsingBrandColor();
    }

    @Override
    public Tab getTab() {
        return hasTab() ? mTab : null;
    }

    @Override
    public boolean hasTab() {
        // TODO(dtrainor, tedchoc): Remove the isInitialized() check when we no longer wait for
        // TAB_CLOSED events to remove this tab.  Otherwise there is a chance we use this tab after
        // {@link ChromeTab#destroy()} is called.
        return mTab != null && mTab.isInitialized();
    }

    @Override
    public String getCurrentUrl() {
        // TODO(yusufo) : Consider using this for all calls from getTab() for accessing url.
        if (!hasTab()) return "";

        // Tab.getUrl() returns empty string if it does not have a URL.
        return getTab().getUrl().trim();
    }

    @Override
    public NewTabPage getNewTabPageForCurrentTab() {
        if (hasTab() && mTab.getNativePage() instanceof NewTabPage) {
            return (NewTabPage) mTab.getNativePage();
        }
        return null;
    }

    @Override
    public UrlBarData getUrlBarData() {
        if (!hasTab()) return UrlBarData.EMPTY;

        String url = getCurrentUrl();
        if (NativePageFactory.isNativePageUrl(url, isIncognito()) || NewTabPage.isNTPUrl(url)) {
            return UrlBarData.EMPTY;
        }

        String formattedUrl = getFormattedFullUrl();
        if (mTab.isFrozen()) return buildUrlBarData(url, formattedUrl);

        if (DomDistillerUrlUtils.isDistilledPage(url)) {
            DomDistillerService domDistillerService =
                    DomDistillerServiceFactory.getForProfile(getProfile());
            String entryIdFromUrl = DomDistillerUrlUtils.getValueForKeyInUrl(url, "entry_id");
            if (!TextUtils.isEmpty(entryIdFromUrl)
                    && domDistillerService.hasEntry(entryIdFromUrl)) {
                String originalUrl = domDistillerService.getUrlForEntry(entryIdFromUrl);
                return buildUrlBarData(
                        DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl));
            }

            String originalUrl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
            if (originalUrl != null) {
                return buildUrlBarData(
                        DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl));
            }
            return buildUrlBarData(url, formattedUrl);
        }

        // A preview URL may be shown before commit when isPreview() would first return true. Always
        // check if the visible URL is a preview, and if so, rewrite it.
        final String previewsOriginalURL = PreviewsAndroidBridge.getInstance().getOriginalURL(url);
        if (!previewsOriginalURL.equals(url)) {
            // Strip the scheme if this is a committed preview.
            return buildUrlBarData(previewsOriginalURL,
                    isPreview() ? UrlUtilities.stripScheme(previewsOriginalURL)
                                : previewsOriginalURL);
        }

        // Strip the scheme from committed preview pages only.
        if (isPreview()) {
            return buildUrlBarData(url, UrlUtilities.stripScheme(url));
        }

        if (isOfflinePage()) {
            String originalUrl = mTab.getOriginalUrl();
            formattedUrl = UrlUtilities.stripScheme(
                    DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl));

            // Clear the editing text for untrusted offline pages.
            if (!OfflinePageUtils.isShowingTrustedOfflinePage(mTab)) {
                return buildUrlBarData(url, formattedUrl, "");
            }

            return buildUrlBarData(url, formattedUrl);
        }

        String searchTerms = getDisplaySearchTerms();
        if (searchTerms != null) {
            // Show the search terms in the omnibox instead of the URL if this is a DSE search URL.
            return buildUrlBarData(url, searchTerms);
        }

        String urlForDisplay = getUrlForDisplay();
        if (!urlForDisplay.equals(formattedUrl)) {
            return buildUrlBarData(url, urlForDisplay, formattedUrl);
        }

        return buildUrlBarData(url, formattedUrl);
    }

    private UrlBarData buildUrlBarData(String url) {
        return buildUrlBarData(url, url, url);
    }

    private UrlBarData buildUrlBarData(String url, String displayText) {
        return buildUrlBarData(url, displayText, displayText);
    }

    private UrlBarData buildUrlBarData(String url, String displayText, String editingText) {
        SpannableStringBuilder spannableDisplayText = new SpannableStringBuilder(displayText);

        if (mNativeLocationBarModelAndroid != 0 && spannableDisplayText.length() > 0
                && shouldEmphasizeUrl()) {
            boolean isInternalPage = false;
            try {
                isInternalPage = UrlUtilities.isInternalScheme(new URI(url));
            } catch (URISyntaxException e) {
                // Ignore as this only is for applying color
            }

            OmniboxUrlEmphasizer.emphasizeUrl(spannableDisplayText, mContext.getResources(),
                    getProfile(), getSecurityLevel(), isInternalPage,
                    !ColorUtils.shouldUseLightForegroundOnBackground(getPrimaryColor()),
                    shouldEmphasizeHttpsScheme());
        }

        return UrlBarData.forUrlAndText(url, spannableDisplayText, editingText);
    }

    /**
     * @return True if the displayed URL should be emphasized, false if the displayed text
     *         already has formatting for emphasis applied.
     */
    private boolean shouldEmphasizeUrl() {
        // If the toolbar shows the publisher URL, it applies its own formatting for emphasis.
        if (mTab == null) return true;

        return !shouldDisplaySearchTerms() && TrustedCdn.getPublisherUrl(mTab) == null;
    }

    /**
     * @return Whether the light security theme should be used.
     */
    @VisibleForTesting
    public boolean shouldEmphasizeHttpsScheme() {
        return !isUsingBrandColor() && !isIncognito();
    }

    @Override
    public String getTitle() {
        if (!hasTab()) return "";

        String title = getTab().getTitle();
        return TextUtils.isEmpty(title) ? title : title.trim();
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public Profile getProfile() {
        Profile lastUsedProfile = Profile.getLastUsedProfile();
        if (mIsIncognito) {
            assert lastUsedProfile.hasOffTheRecordProfile();
            return lastUsedProfile.getOffTheRecordProfile();
        }
        return lastUsedProfile.getOriginalProfile();
    }

    /**
     * Sets the primary color and changes the state for isUsingBrandColor.
     * @param color The primary color for the current tab.
     */
    public void setPrimaryColor(int color) {
        mPrimaryColor = color;
        updateUsingBrandColor();
    }

    private void updateUsingBrandColor() {
        Context context = ContextUtils.getApplicationContext();
        mIsUsingBrandColor = !isIncognito()
                && mPrimaryColor
                        != ColorUtils.getDefaultThemeColor(context.getResources(), isIncognito())
                && hasTab() && !mTab.isNativePage();
    }

    @Override
    public int getPrimaryColor() {
        return mPrimaryColor;
    }

    @Override
    public boolean isUsingBrandColor() {
        return mIsUsingBrandColor;
    }

    @Override
    public boolean isOfflinePage() {
        return hasTab() && OfflinePageUtils.isOfflinePage(mTab);
    }

    @Override
    public boolean isPreview() {
        return hasTab() && mTab.isPreview();
    }

    @Override
    public int getSecurityLevel() {
        Tab tab = getTab();
        return getSecurityLevel(tab, isOfflinePage(), TrustedCdn.getPublisherUrl(tab));
    }

    @Override
    public int getSecurityIconResource(boolean isTablet) {
        // If we're showing a query in the omnibox, and the security level is high enough to show
        // the search icon, return that instead of the security icon.
        if (shouldDisplaySearchTerms()) {
            return R.drawable.omnibox_search;
        }
        return getSecurityIconResource(getSecurityLevel(), !isTablet, isOfflinePage(), isPreview());
    }

    @VisibleForTesting
    @ConnectionSecurityLevel
    static int getSecurityLevel(Tab tab, boolean isOfflinePage, @Nullable String publisherUrl) {
        if (tab == null || isOfflinePage) {
            return ConnectionSecurityLevel.NONE;
        }

        int securityLevel = tab.getSecurityLevel();
        if (publisherUrl != null) {
            assert securityLevel != ConnectionSecurityLevel.DANGEROUS;
            return (URI.create(publisherUrl).getScheme().equals(UrlConstants.HTTPS_SCHEME))
                    ? ConnectionSecurityLevel.SECURE
                    : ConnectionSecurityLevel.HTTP_SHOW_WARNING;
        }
        return securityLevel;
    }

    @VisibleForTesting
    @DrawableRes
    static int getSecurityIconResource(
            int securityLevel, boolean isSmallDevice, boolean isOfflinePage, boolean isPreview) {
        // Checking for a preview first because one possible preview type is showing an offline page
        // on a slow connection. In this case, the previews UI takes precedence.
        if (isPreview) {
            return R.drawable.preview_pin_round;
        } else if (isOfflinePage) {
            return R.drawable.offline_pin_round;
        }

        switch (securityLevel) {
            case ConnectionSecurityLevel.NONE:
                return isSmallDevice ? 0 : R.drawable.omnibox_info;
            case ConnectionSecurityLevel.HTTP_SHOW_WARNING:
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.drawable.omnibox_https_invalid;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
            case ConnectionSecurityLevel.EV_SECURE:
                return R.drawable.omnibox_https_valid;
            default:
                assert false;
        }
        return 0;
    }

    @Override
    public @ColorRes int getSecurityIconColorStateList() {
        int securityLevel = getSecurityLevel();
        int color = getPrimaryColor();
        boolean needLightIcon = ColorUtils.shouldUseLightForegroundOnBackground(color);

        if (isIncognito() || needLightIcon) {
            // For a dark theme color, use light icons.
            return ColorUtils.getThemedToolbarIconTintRes(true);
        }

        if (isPreview()) {
            // There will never be a preview in incognito. Always use the darker color rather than
            // incorporating with the block above.
            return R.color.locationbar_status_preview_color;
        }

        if (!hasTab() || isUsingBrandColor()
                || ChromeFeatureList.isEnabled(
                        ChromeFeatureList.OMNIBOX_HIDE_SCHEME_IN_STEADY_STATE)
                || ChromeFeatureList.isEnabled(
                        ChromeFeatureList.OMNIBOX_HIDE_TRIVIAL_SUBDOMAINS_IN_STEADY_STATE)) {
            // For theme colors which are not dark and are also not
            // light enough to warrant an opaque URL bar, use dark
            // icons.
            return ColorUtils.getThemedToolbarIconTintRes(false);
        }

        // TODO(https://crbug.com/940134): Change the color here and also #needLightIcon logic.
        if (securityLevel == ConnectionSecurityLevel.DANGEROUS) {
            // For the default toolbar color, use a green or red icon.
            assert !shouldDisplaySearchTerms();
            return R.color.google_red_700;
        }

        if (!shouldDisplaySearchTerms()
                && (securityLevel == ConnectionSecurityLevel.SECURE
                        || securityLevel == ConnectionSecurityLevel.EV_SECURE)) {
            return R.color.google_green_700;
        }

        return ColorUtils.getThemedToolbarIconTintRes(false);
    }

    @Override
    public boolean shouldDisplaySearchTerms() {
        return getDisplaySearchTerms() != null && !isPreview();
    }

    /**
     * If the current tab state is eligible for displaying the search query terms instead of the
     * URL, this extracts the query terms from the current URL.
     *
     * @return The search terms. Returns null if the tab is ineligible to display the search terms
     *         instead of the URL.
     */
    public String getDisplaySearchTerms() {
        if (mNativeLocationBarModelAndroid == 0) return null;
        if (mTab != null && !(mTab.getActivity() instanceof ChromeTabbedActivity)) return null;
        return nativeGetDisplaySearchTerms(mNativeLocationBarModelAndroid);
    }

    /** @return The formatted URL suitable for editing. */
    public String getFormattedFullUrl() {
        if (mNativeLocationBarModelAndroid == 0) return "";
        return nativeGetFormattedFullURL(mNativeLocationBarModelAndroid);
    }

    /** @return The formatted URL suitable for display only. */
    public String getUrlForDisplay() {
        if (mNativeLocationBarModelAndroid == 0) return "";
        return nativeGetURLForDisplay(mNativeLocationBarModelAndroid);
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativeLocationBarModelAndroid);
    private native String nativeGetFormattedFullURL(long nativeLocationBarModelAndroid);
    private native String nativeGetURLForDisplay(long nativeLocationBarModelAndroid);
    private native String nativeGetDisplaySearchTerms(long nativeLocationBarModelAndroid);
}
