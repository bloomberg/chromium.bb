// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ListView;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionViewBinder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that handles the interactions with the autocomplete system.
 */
public class AutocompleteCoordinatorImpl implements AutocompleteCoordinator {
    private final ViewGroup mParent;
    private AutocompleteMediator mMediator;

    private ListView mListView;

    /**
     * See {@link AutocompleteCoordinatorFactory#createAutocompleteCoordinator}.
     *
     * Keep this constructor protected so clients use the factory instead.
     */
    @VisibleForTesting
    protected AutocompleteCoordinatorImpl(ViewGroup parent, AutocompleteDelegate delegate,
            OmniboxSuggestionListEmbedder listEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
        mParent = parent;
        Context context = parent.getContext();

        PropertyModel listModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        listModel.set(SuggestionListProperties.EMBEDDER, listEmbedder);
        listModel.set(SuggestionListProperties.VISIBLE, false);

        ViewProvider<SuggestionListViewHolder> viewProvider = createViewProvider(context);
        viewProvider.whenLoaded((holder) -> { mListView = holder.listView; });
        LazyConstructionPropertyMcp.create(listModel, SuggestionListProperties.VISIBLE,
                viewProvider, SuggestionListViewBinder::bind);

        mMediator =
                new AutocompleteMediator(context, delegate, urlBarEditingTextProvider, listModel);
    }

    @Override
    public void destroy() {
        mMediator.destroy();
        mMediator = null;
    }

    private ViewProvider<SuggestionListViewHolder> createViewProvider(Context context) {
        return new ViewProvider<SuggestionListViewHolder>() {
            private List<Callback<SuggestionListViewHolder>> mCallbacks = new ArrayList<>();
            private SuggestionListViewHolder mHolder;

            @Override
            public void inflate() {
                ViewGroup container = (ViewGroup) ((ViewStub) mParent.getRootView().findViewById(
                                                           R.id.omnibox_results_container_stub))
                                              .inflate();
                OmniboxSuggestionsList list;
                try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                    list = new OmniboxSuggestionsList(context);
                }

                // Start with visibility GONE to ensure that show() is called.
                // http://crbug.com/517438
                list.setVisibility(View.GONE);
                ModelListAdapter adapter = new ModelListAdapter(context);
                list.setAdapter(adapter);
                list.setClipToPadding(false);

                // Register a view type for a default omnibox suggestion.
                // Note: clang-format does a bad job formatting lambdas so we turn it off here.
                // clang-format off
                adapter.registerType(
                        OmniboxSuggestionUiType.DEFAULT,
                        () -> new SuggestionView(mListView.getContext()),
                        SuggestionViewViewBinder::bind);

                adapter.registerType(
                        OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                        () -> EditUrlSuggestionProcessor.createView(mListView.getContext()),
                        EditUrlSuggestionViewBinder::bind);

                adapter.registerType(
                        OmniboxSuggestionUiType.ANSWER_SUGGESTION,
                        () -> (AnswerSuggestionView) LayoutInflater.from(mListView.getContext())
                                .inflate(R.layout.omnibox_answer_suggestion, null),
                        AnswerSuggestionViewBinder::bind);

                adapter.registerType(
                        OmniboxSuggestionUiType.ENTITY_SUGGESTION,
                        () -> (EntitySuggestionView) LayoutInflater.from(mListView.getContext())
                                .inflate(R.layout.omnibox_entity_suggestion, null),
                        EntitySuggestionViewBinder::bind);
                // clang-format on

                mHolder = new SuggestionListViewHolder(container, list, adapter);

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

    @Override
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mMediator.setToolbarDataProvider(toolbarDataProvider);
    }

    @Override
    public void setAutocompleteProfile(Profile profile) {
        mMediator.setAutocompleteProfile(profile);
    }

    @Override
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mMediator.setWindowAndroid(windowAndroid);
    }

    @Override
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mMediator.setActivityTabProvider(provider);
    }

    @Override
    public void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mMediator.setShouldPreventOmniboxAutocomplete(prevent);
    }

    @Override
    public int getSuggestionCount() {
        return mMediator.getSuggestionCount();
    }

    @Override
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    @Override
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
    }

    @Override
    public void onVoiceResults(
            @Nullable List<LocationBarVoiceRecognitionHandler.VoiceResult> results) {
        mMediator.onVoiceResults(results);
    }

    @Override
    public long getCurrentNativeAutocompleteResult() {
        return mMediator.getCurrentNativeAutocompleteResult();
    }

    @Override
    public void updateSuggestionListLayoutDirection() {
        mMediator.setLayoutDirection(ViewCompat.getLayoutDirection(mParent));
    }

    @Override
    public void updateVisualsForState(boolean useDarkColors, boolean isIncognito) {
        mMediator.updateVisualsForState(useDarkColors, isIncognito);
    }

    @Override
    public void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mMediator.setShowCachedZeroSuggestResults(showCachedZeroSuggestResults);
    }

    @Override
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        boolean isShowingList = mListView != null && mListView.isShown();

        boolean isUpOrDown = KeyNavigationUtil.isGoUpOrDown(event);
        if (isShowingList && mMediator.getSuggestionCount() > 0 && isUpOrDown) {
            mMediator.allowPendingItemSelection();
        }
        boolean isValidListKey = isUpOrDown || KeyNavigationUtil.isGoRight(event)
                || KeyNavigationUtil.isEnter(event);
        if (isShowingList && isValidListKey && mListView.onKeyDown(keyCode, event)) return true;
        if (KeyNavigationUtil.isEnter(event) && mParent.getVisibility() == View.VISIBLE) {
            mMediator.loadTypedOmniboxText(event.getEventTime());
            return true;
        }
        return false;
    }

    @Override
    public void onTextChangedForAutocomplete() {
        mMediator.onTextChangedForAutocomplete();
    }

    @Override
    public void startAutocompleteForQuery(String query) {
        mMediator.startAutocompleteForQuery(query);
    }

    @VisibleForTesting
    ListView getSuggestionList() {
        return mListView;
    }

    @VisibleForTesting
    void setAutocompleteController(AutocompleteController controller) {
        mMediator.setAutocompleteController(controller);
    }

    @VisibleForTesting
    OnSuggestionsReceivedListener getSuggestionsReceivedListenerForTest() {
        return mMediator;
    }
}
