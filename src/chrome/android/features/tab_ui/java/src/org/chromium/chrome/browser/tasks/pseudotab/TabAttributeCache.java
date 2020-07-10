// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;

/**
 * Cache for attributes of {@link PseudoTab} to be available before native is ready.
 */
public class TabAttributeCache {
    private static final String PREFERENCES_NAME = "tab_attribute_cache";
    private static SharedPreferences sPref;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    interface LastSearchTermProvider {
        String getLastSearchTerm(Tab tab);
    }

    private static LastSearchTermProvider sLastSearchTermProviderForTests;

    private static SharedPreferences getSharedPreferences() {
        if (sPref == null) {
            sPref = ContextUtils.getApplicationContext().getSharedPreferences(
                    PREFERENCES_NAME, Context.MODE_PRIVATE);
        }
        return sPref;
    }

    /**
     * Create a TabAttributeCache instance to observe tab attribute changes.
     *
     * Note that querying tab attributes doesn't rely on having an instance.
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     */
    public TabAttributeCache(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onUrlUpdated(Tab tab) {
                if (tab.isIncognito()) return;
                String url = tab.getUrl();
                cacheUrl(tab.getId(), url);
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                if (tab.isIncognito()) return;
                String title = tab.getTitle();
                cacheTitle(tab.getId(), title);
            }

            @Override
            public void onRootIdChanged(Tab tab, int newRootId) {
                if (tab.isIncognito()) return;
                assert newRootId == ((TabImpl) tab).getRootId();
                cacheRootId(tab.getId(), newRootId);
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (tab.isIncognito()) return;
                if (!navigationHandle.isInMainFrame()) return;
                if (tab.getWebContents() == null) return;
                // TODO(crbug.com/1048255): skip cacheLastSearchTerm() according to
                //  isValidSearchFormUrl() and PageTransition.GENERATED for optimization.
                cacheLastSearchTerm(tab);
            }
        };

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                int id = tab.getId();
                getSharedPreferences()
                        .edit()
                        .remove(getUrlKey(id))
                        .remove(getTitleKey(id))
                        .remove(getRootIdKey(id))
                        .remove(getLastSearchTermKey(id))
                        .apply();
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                // TODO(wychen): after this cache is enabled by default, we only need to populate it
                //  once.
                TabModelFilter filter =
                        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
                for (int i = 0; i < filter.getCount(); i++) {
                    Tab tab = filter.getTabAt(i);
                    cacheUrl(tab.getId(), tab.getUrl());
                    cacheTitle(tab.getId(), tab.getTitle());
                    cacheRootId(tab.getId(), ((TabImpl) tab).getRootId());
                }
                Tab currentTab = mTabModelSelector.getCurrentTab();
                if (currentTab != null) cacheLastSearchTerm(currentTab);
                filter.addObserver(mTabModelObserver);
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    private static String getTitleKey(int id) {
        return id + "_title";
    }

    /**
     * Get the title of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @return The title
     */
    public static String getTitle(int id) {
        return getSharedPreferences().getString(getTitleKey(id), "");
    }

    private static void cacheTitle(int id, String title) {
        getSharedPreferences().edit().putString(getTitleKey(id), title).apply();
    }

    /**
     * Set the title of a {@link PseudoTab}. Only for testing.
     * @param id The ID of the {@link PseudoTab}.
     * @param title The title
     */
    static void setTitleForTesting(int id, String title) {
        cacheTitle(id, title);
    }

    private static String getUrlKey(int id) {
        return id + "_url";
    }

    /**
     * Get the URL of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @return The URL
     */
    public static String getUrl(int id) {
        return getSharedPreferences().getString(getUrlKey(id), "");
    }

    private static void cacheUrl(int id, String url) {
        getSharedPreferences().edit().putString(getUrlKey(id), url).apply();
    }

    /**
     * Set the URL of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @param url The URL
     */
    static void setUrlForTesting(int id, String url) {
        cacheUrl(id, url);
    }

    private static String getRootIdKey(int id) {
        return id + "_rootID";
    }

    /**
     * Get the root ID of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @return The root ID
     */
    public static int getRootId(int id) {
        return getSharedPreferences().getInt(getRootIdKey(id), Tab.INVALID_TAB_ID);
    }

    private static void cacheRootId(int id, int rootId) {
        getSharedPreferences().edit().putInt(getRootIdKey(id), rootId).apply();
    }

    /**
     * Set the root ID for a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @param rootId The root ID
     */
    static void setRootIdForTesting(int id, int rootId) {
        cacheRootId(id, rootId);
    }

    private static String getLastSearchTermKey(int id) {
        return id + "_last_search_term";
    }

    /**
     * Get the last search term of the default search engine of a {@link PseudoTab} in the
     * navigation stack.
     *
     * @param id The ID of the {@link PseudoTab}.
     * @return The last search term. Null if none.
     */
    public static @Nullable String getLastSearchTerm(int id) {
        return getSharedPreferences().getString(getLastSearchTermKey(id), null);
    }

    private static void cacheLastSearchTerm(Tab tab) {
        if (tab.getWebContents() == null) return;
        cacheLastSearchTerm(tab.getId(), findLastSearchTerm(tab));
    }

    private static void cacheLastSearchTerm(int id, String searchTerm) {
        getSharedPreferences().edit().putString(getLastSearchTermKey(id), searchTerm).apply();
    }

    /**
     * Find the latest search term from the navigation stack.
     * @param tab The tab to find from.
     * @return The search term. Null for no results.
     */
    @VisibleForTesting
    static @Nullable String findLastSearchTerm(Tab tab) {
        if (sLastSearchTermProviderForTests != null) {
            return sLastSearchTermProviderForTests.getLastSearchTerm(tab);
        }
        assert tab.getWebContents() != null;
        NavigationController controller = tab.getWebContents().getNavigationController();
        NavigationHistory history = controller.getNavigationHistory();

        if (!TextUtils.isEmpty(
                    TemplateUrlServiceFactory.get().getSearchQueryForUrl(tab.getUrl()))) {
            // If we are already at a search result page, do not show the last search term.
            return null;
        }

        for (int i = history.getCurrentEntryIndex() - 1; i >= 0; i--) {
            String url = history.getEntryAtIndex(i).getOriginalUrl();
            String query = TemplateUrlServiceFactory.get().getSearchQueryForUrl(url);
            if (!TextUtils.isEmpty(query)) {
                return removeEscapedCodePoints(query);
            }
        }
        return null;
    }

    /**
     * {@link TemplateUrlService#getSearchQueryForUrl(String)} can leave some code points
     * unescaped for security reasons. See ShouldUnescapeCodePoint().
     * In our use case, dropping the unescaped code points shouldn't introduce security issues,
     * and loss of information is fine because the string is not going to be used other than
     * showing in the UI.
     * @return the rest of code points
     */
    @VisibleForTesting
    static String removeEscapedCodePoints(String string) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < string.length(); i++) {
            if (string.charAt(i) != '%' || i + 2 >= string.length()) {
                sb.append(string.charAt(i));
                continue;
            }
            if (Character.digit(string.charAt(i + 1), 16) == -1
                    || Character.digit(string.charAt(i + 2), 16) == -1) {
                sb.append(string.charAt(i));
                continue;
            }
            i += 2;
        }
        return sb.toString();
    }

    /**
     * Set the LastSearchTermProvider for testing.
     * @param lastSearchTermProvider The mocking object.
     */
    @VisibleForTesting
    static void setLastSearchTermMockForTesting(LastSearchTermProvider lastSearchTermProvider) {
        sLastSearchTermProviderForTests = lastSearchTermProvider;
    }

    /**
     * Set the last search term for a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @param searchTerm The last search term
     */
    @VisibleForTesting
    public static void setLastSearchTermForTesting(int id, String searchTerm) {
        cacheLastSearchTerm(id, searchTerm);
    }

    /**
     * Clear everything in the storage.
     */
    @VisibleForTesting
    public static void clearAllForTesting() {
        getSharedPreferences().edit().clear().apply();
    }

    /**
     * Remove all the observers.
     */
    public void destroy() {
        mTabModelSelectorTabObserver.destroy();
        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false).removeObserver(
                mTabModelObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }
}
