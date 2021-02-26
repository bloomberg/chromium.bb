// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.MostVisitedTilesProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that handles the interactions with the autocomplete system.
 */
public class AutocompleteCoordinator implements UrlFocusChangeListener, UrlTextChangeListener {
    private final ViewGroup mParent;
    private OmniboxQueryTileCoordinator mQueryTileCoordinator;
    private AutocompleteMediator mMediator;
    private OmniboxSuggestionsDropdown mDropdown;

    public AutocompleteCoordinator(ViewGroup parent, AutocompleteDelegate delegate,
            OmniboxSuggestionsDropdownEmbedder dropdownEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
        mParent = parent;
        Context context = parent.getContext();

        PropertyModel listModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        ModelList listItems = new ModelList();

        listModel.set(SuggestionListProperties.EMBEDDER, dropdownEmbedder);
        listModel.set(SuggestionListProperties.VISIBLE, false);
        listModel.set(SuggestionListProperties.SUGGESTION_MODELS, listItems);

        mQueryTileCoordinator = new OmniboxQueryTileCoordinator(context, this::onTileSelected);
        mMediator = new AutocompleteMediator(context, delegate, urlBarEditingTextProvider,
                new AutocompleteController(), listModel, new Handler());
        mMediator.initDefaultProcessors(mQueryTileCoordinator::setTiles);

        listModel.set(SuggestionListProperties.OBSERVER, mMediator);

        ViewProvider<SuggestionListViewHolder> viewProvider =
                createViewProvider(context, listItems);
        viewProvider.whenLoaded((holder) -> { mDropdown = holder.dropdown; });
        LazyConstructionPropertyMcp.create(listModel, SuggestionListProperties.VISIBLE,
                viewProvider, SuggestionListViewBinder::bind);

        // https://crbug.com/966227 Set initial layout direction ahead of inflating the suggestions.
        updateSuggestionListLayoutDirection();
    }

    /**
     * Clean up resources used by this class.
     */
    public void destroy() {
        mQueryTileCoordinator.destroy();
        mQueryTileCoordinator = null;
        mMediator.destroy();
        mMediator = null;
    }

    private ViewProvider<SuggestionListViewHolder> createViewProvider(
            Context context, MVCListAdapter.ModelList modelList) {
        return new ViewProvider<SuggestionListViewHolder>() {
            private List<Callback<SuggestionListViewHolder>> mCallbacks = new ArrayList<>();
            private SuggestionListViewHolder mHolder;

            @Override
            public void inflate() {
                OmniboxSuggestionsDropdown dropdown;
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    dropdown = new OmniboxSuggestionsDropdown(context);
                }

                // Start with visibility GONE to ensure that show() is called.
                // http://crbug.com/517438
                dropdown.getViewGroup().setVisibility(View.GONE);
                dropdown.getViewGroup().setClipToPadding(false);

                OmniboxSuggestionsDropdownAdapter adapter =
                        new OmniboxSuggestionsDropdownAdapter(modelList);
                dropdown.setAdapter(adapter);

                // Note: clang-format does a bad job formatting lambdas so we turn it off here.
                // clang-format off
                // Register a view type for a default omnibox suggestion.
                adapter.registerType(
                        OmniboxSuggestionUiType.DEFAULT,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                        new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                        parent -> new EditUrlSuggestionView(parent.getContext()),
                        new EditUrlSuggestionViewBinder());

                adapter.registerType(
                        OmniboxSuggestionUiType.ANSWER_SUGGESTION,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_answer_suggestion),
                        new BaseSuggestionViewBinder<View>(AnswerSuggestionViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.ENTITY_SUGGESTION,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_entity_suggestion),
                        new BaseSuggestionViewBinder<View>(EntitySuggestionViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.TAIL_SUGGESTION,
                        parent -> new BaseSuggestionView<TailSuggestionView>(
                                new TailSuggestionView(parent.getContext())),
                        new BaseSuggestionViewBinder<TailSuggestionView>(
                                TailSuggestionViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                        new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.TILE_SUGGESTION,
                        parent -> mQueryTileCoordinator.createView(parent.getContext()),
                        mQueryTileCoordinator::bind);

                adapter.registerType(
                        OmniboxSuggestionUiType.TILE_NAVSUGGEST,
                        MostVisitedTilesProcessor::createView,
                        BaseCarouselSuggestionViewBinder::bind);

                adapter.registerType(
                        OmniboxSuggestionUiType.HEADER,
                        parent -> new HeaderView(parent.getContext()),
                        HeaderViewBinder::bind);
                // clang-format on

                ViewGroup container = (ViewGroup) ((ViewStub) mParent.getRootView().findViewById(
                                                           R.id.omnibox_results_container_stub))
                                              .inflate();

                mHolder = new SuggestionListViewHolder(container, dropdown);
                for (int i = 0; i < mCallbacks.size(); i++) {
                    mCallbacks.get(i).onResult(mHolder);
                }
                mCallbacks = null;
            }

            @Override
            public void whenLoaded(Callback<SuggestionListViewHolder> callback) {
                if (mHolder != null) {
                    callback.onResult(mHolder);
                    return;
                }
                mCallbacks.add(callback);
            }
        };
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mMediator.onUrlFocusChange(hasFocus);
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        mMediator.onUrlAnimationFinished(hasFocus);
    }

    /**
     * Provides data and state for the toolbar component.
     * @param locationBarDataProvider The data provider.
     */
    public void setLocationBarDataProvider(LocationBarDataProvider locationBarDataProvider) {
        mMediator.setLocationBarDataProvider(locationBarDataProvider);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    public void setAutocompleteProfile(Profile profile) {
        mMediator.setAutocompleteProfile(profile);
        mQueryTileCoordinator.setProfile(profile);
    }

    /**
     * Set the WindowAndroid instance associated with the containing Activity.
     */
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mMediator.setWindowAndroid(windowAndroid);
    }

    /**
     * @param provider A means of accessing the activity's tab.
     */
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mMediator.setActivityTabProvider(provider);
    }

    /**
     * @param shareDelegateSupplier A means of accessing the sharing feature.
     */
    public void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier) {
        mMediator.setShareDelegateSupplier(shareDelegateSupplier);
    }

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
    public void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mMediator.setShouldPreventOmniboxAutocomplete(prevent);
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mMediator.getSuggestionCount();
    }

    /**
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    public void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results) {
        mMediator.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     * TODO(ender): Figure out how to remove this.
     */
    public long getCurrentNativeAutocompleteResult() {
        return mMediator.getCurrentNativeAutocompleteResult();
    }

    /**
     * Update the layout direction of the suggestion list based on the parent layout direction.
     */
    public void updateSuggestionListLayoutDirection() {
        mMediator.setLayoutDirection(ViewCompat.getLayoutDirection(mParent));
    }

    /**
     * Update the visuals of the autocomplete UI.
     * @param useDarkColors Whether dark colors should be applied to the UI.
     * @param isIncognito Whether the UI is for incognito mode or not.
     */
    public void updateVisualsForState(boolean useDarkColors, boolean isIncognito) {
        mMediator.updateVisualsForState(useDarkColors, isIncognito);
    }

    /**
     * Sets to show cached zero suggest results. This will start both caching zero suggest results
     * in shared preferences and also attempt to show them when appropriate without needing native
     * initialization.
     * @param showCachedZeroSuggestResults Whether cached zero suggest should be shown.
     */
    public void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mMediator.setShowCachedZeroSuggestResults(showCachedZeroSuggestResults);
    }

    /**
     * Handle the key events associated with the suggestion list.
     *
     * @param keyCode The keycode representing what key was interacted with.
     * @param event The key event containing all meta-data associated with the event.
     * @return Whether the key event was handled.
     */
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        boolean isShowingList = mDropdown != null && mDropdown.getViewGroup().isShown();

        boolean isAnyDirection = KeyNavigationUtil.isGoAnyDirection(event);
        if (isShowingList && mMediator.getSuggestionCount() > 0 && isAnyDirection) {
            mMediator.allowPendingItemSelection();
        }
        boolean isValidListKey = isAnyDirection || KeyNavigationUtil.isEnter(event);
        if (isShowingList && isValidListKey && mDropdown.getViewGroup().onKeyDown(keyCode, event)) {
            return true;
        }
        if (KeyNavigationUtil.isEnter(event) && mParent.getVisibility() == View.VISIBLE) {
            mMediator.loadTypedOmniboxText(event.getEventTime());
            return true;
        }
        return false;
    }

    @Override
    public void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete) {
        mMediator.onTextChanged(textWithoutAutocomplete, textWithAutocomplete);
    }

    /**
     * Trigger autocomplete for the given query.
     */
    public void startAutocompleteForQuery(String query) {
        mMediator.startAutocompleteForQuery(query);
    }

    /**
     * Given a search query, this will attempt to see if the query appears to be portion of a
     * properly formed URL.  If it appears to be a URL, this will return the fully qualified
     * version (i.e. including the scheme, etc...).  If the query does not appear to be a URL,
     * this will return null.
     *
     * TODO(crbug.com/966424): Fix the dependency issue and remove this method.
     *
     * @param query The query to be expanded into a fully qualified URL if appropriate.
     * @return The fully qualified URL or null.
     */
    @Deprecated
    public static String qualifyPartialURLQuery(String query) {
        return AutocompleteControllerJni.get().qualifyPartialURLQuery(query);
    }

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     */
    public void prefetchZeroSuggestResults() {
        AutocompleteControllerJni.get().prefetchZeroSuggestResults();
    }

    /** @return Suggestions Dropdown view, showing the list of suggestions. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public OmniboxSuggestionsDropdown getSuggestionsDropdownForTest() {
        return mDropdown;
    }

    /** @param controller The instance of AutocompleteController to be used. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void setAutocompleteControllerForTest(AutocompleteController controller) {
        mMediator.setAutocompleteControllerForTest(controller);
    }

    /** @return The current receiving OnSuggestionsReceived events. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public OnSuggestionsReceivedListener getSuggestionsReceivedListenerForTest() {
        return mMediator;
    }

    /** @return The ModelList for the currently shown suggestions. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public ModelList getSuggestionModelListForTest() {
        return mMediator.getSuggestionModelListForTest();
    }

    private void onTileSelected(QueryTile queryTile) {
        mMediator.onQueryTileSelected(queryTile);
    }
}
