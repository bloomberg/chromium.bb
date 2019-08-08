// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.ViewGroup;

import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;

/**
 * Factory to create AutocompleteCoordinator instances.
 */
public class AutocompleteCoordinatorFactory {
    private AutocompleteCoordinatorFactory() {}

    /**
     * Constructs a coordinator for the autocomplete system.
     *
     * @param parent The UI parent component for the autocomplete UI.
     * @param delegate The delegate to fulfill additional autocomplete requirements.
     * @param listEmbedder The embedder for controlling the display constraints of the suggestions
     *                     list.
     * @param urlBarEditingTextProvider Provider of editing text state from the UrlBar.
     */
    public static AutocompleteCoordinator createAutocompleteCoordinator(ViewGroup parent,
            AutocompleteDelegate delegate, OmniboxSuggestionListEmbedder listEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
        return new AutocompleteCoordinatorImpl(
                parent, delegate, listEmbedder, urlBarEditingTextProvider);
    }
}
