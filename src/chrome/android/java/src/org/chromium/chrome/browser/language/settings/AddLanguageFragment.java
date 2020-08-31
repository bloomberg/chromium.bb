// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;

import androidx.appcompat.widget.SearchView;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Fragment with a {@link RecyclerView} containing a list of languages that users may add to their
 * accept languages. There is a {@link SearchView} on its Actionbar to make a quick lookup.
 */
public class AddLanguageFragment extends Fragment {
    static final String INTENT_NEW_ACCEPT_LANGUAGE = "AddLanguageFragment.NewLanguage";

    /**
     * A host to launch AddLanguageFragment and receive the result.
     */
    interface Launcher {
        /**
         * Launches AddLanguageFragment.
         */
        void launchAddLanguage();
    }

    private class LanguageSearchListAdapter extends LanguageListBaseAdapter {
        LanguageSearchListAdapter(Context context) {
            super(context);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);
            ((LanguageRowViewHolder) holder)
                    .setItemClickListener(getItemByPosition(position), mItemClickListener);
        }

        /**
         * Called to perform a search.
         * @param query The text to search for.
         */
        private void search(String query) {
            if (TextUtils.isEmpty(query)) {
                setDisplayedLanguages(mFullLanguageList);
                return;
            }

            Locale locale = Locale.getDefault();
            query = query.trim().toLowerCase(locale);
            List<LanguageItem> results = new ArrayList<>();
            for (LanguageItem item : mFullLanguageList) {
                // TODO(crbug/783049): Consider searching in item's native display name and
                // language code too.
                if (item.getDisplayName().toLowerCase(locale).contains(query)) {
                    results.add(item);
                }
            }
            setDisplayedLanguages(results);
        }
    }

    // The view for searching the list of items.
    private SearchView mSearchView;

    // If not blank, represents a substring to use to search for language names.
    private String mSearch;

    private RecyclerView mRecyclerView;
    private LanguageSearchListAdapter mAdapter;
    private List<LanguageItem> mFullLanguageList;
    private LanguageListBaseAdapter.ItemClickListener mItemClickListener;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.add_language);
        setHasOptionsMenu(true);
        LanguagesManager.recordImpression(
                LanguagesManager.LanguageSettingsPageType.PAGE_ADD_LANGUAGE);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Inflate the layout for this fragment.
        View view = inflater.inflate(R.layout.add_languages_main, container, false);
        mSearch = "";
        final Activity activity = getActivity();

        mRecyclerView = (RecyclerView) view.findViewById(R.id.language_list);
        LinearLayoutManager layoutManager = new LinearLayoutManager(activity);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(activity, layoutManager.getOrientation()));

        mFullLanguageList = LanguagesManager.getInstance().getLanguageItemsExcludingUserAccept();
        mItemClickListener = item -> {
            Intent intent = new Intent();
            intent.putExtra(INTENT_NEW_ACCEPT_LANGUAGE, item.getCode());
            activity.setResult(Activity.RESULT_OK, intent);
            activity.finish();
        };
        mAdapter = new LanguageSearchListAdapter(activity);

        mRecyclerView.setAdapter(mAdapter);
        mAdapter.setDisplayedLanguages(mFullLanguageList);
        mRecyclerView.getViewTreeObserver().addOnScrollChangedListener(
                SettingsUtils.getShowShadowOnScrollListener(
                        mRecyclerView, view.findViewById(R.id.shadow)));
        return view;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.languages_action_bar_menu, menu);

        mSearchView = (SearchView) menu.findItem(R.id.search).getActionView();
        mSearchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);

        mSearchView.setOnCloseListener(() -> {
            mSearch = "";
            mAdapter.setDisplayedLanguages(mFullLanguageList);
            return false;
        });

        mSearchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                return true;
            }

            @Override
            public boolean onQueryTextChange(String query) {
                if (TextUtils.isEmpty(query) || TextUtils.equals(query, mSearch)) {
                    return true;
                }

                mSearch = query;
                mAdapter.search(mSearch);
                return true;
            }
        });
    }
}
