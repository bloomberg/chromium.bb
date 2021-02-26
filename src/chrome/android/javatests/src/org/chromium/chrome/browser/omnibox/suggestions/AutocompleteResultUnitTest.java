// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteResult.GroupDetails;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link AutocompleteResult}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AutocompleteResultUnitTest {
    private OmniboxSuggestion buildSuggestionForIndex(int index) {
        return OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Dummy Suggestion " + index)
                .setDescription("Dummy Description " + index)
                .build();
    }

    @Test
    @SmallTest
    public void autocompleteResult_sameContentsAreEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        // Element 0: 2 subtypes
        list1.get(0).getSubtypes().add(10);
        list1.get(0).getSubtypes().add(17);
        list2.get(0).getSubtypes().add(10);
        list2.get(0).getSubtypes().add(17);

        // Element 1: 0 subtypes.
        // Element 2: 1 subtype.
        list1.get(2).getSubtypes().add(4);
        list2.get(2).getSubtypes().add(4);

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", true));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", true));

        AutocompleteResult res1 = new AutocompleteResult(list1, groupsDetails1);
        AutocompleteResult res2 = new AutocompleteResult(list2, groupsDetails2);

        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_itemsOutOfOrderAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(2), buildSuggestionForIndex(1), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", true));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", true));

        AutocompleteResult res1 = new AutocompleteResult(list1, groupsDetails1);
        AutocompleteResult res2 = new AutocompleteResult(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_missingGroupsDetailsAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", true));
        groupsDetails1.put(20, new GroupDetails("Test", false));

        groupsDetails2.put(10, new GroupDetails("Hello", true));

        AutocompleteResult res1 = new AutocompleteResult(list1, groupsDetails1);
        AutocompleteResult res2 = new AutocompleteResult(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_groupsWithDifferentDefaultExpandedStateAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", true));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", false));

        AutocompleteResult res1 = new AutocompleteResult(list1, groupsDetails1);
        AutocompleteResult res2 = new AutocompleteResult(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_extraGroupsDetailsAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", false));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", false));
        groupsDetails2.put(30, new GroupDetails("Yikes", false));

        AutocompleteResult res1 = new AutocompleteResult(list1, groupsDetails1);
        AutocompleteResult res2 = new AutocompleteResult(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_differentItemsAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(4));

        AutocompleteResult res1 = new AutocompleteResult(list1, null);
        AutocompleteResult res2 = new AutocompleteResult(list2, null);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_differentGroupsDetailsAreNotEqual() {
        List<OmniboxSuggestion> list = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails3 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", false));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(15, new GroupDetails("Test", false));

        groupsDetails3.put(10, new GroupDetails("Hello", false));
        groupsDetails3.put(20, new GroupDetails("Test 2", false));

        AutocompleteResult res1 = new AutocompleteResult(list, groupsDetails1);
        AutocompleteResult res2 = new AutocompleteResult(list, groupsDetails2);
        AutocompleteResult res3 = new AutocompleteResult(list, groupsDetails3);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1, res3);
        Assert.assertNotEquals(res2, res3);
    }

    @Test
    @SmallTest
    public void autocompleteResult_differentSubtypesAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(10)
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(17)
                        .build());

        List<OmniboxSuggestion> list2 = Arrays.asList(
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(10)
                        .build(),
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(4)
                        .build());

        AutocompleteResult res1 = new AutocompleteResult(list1, null);
        AutocompleteResult res2 = new AutocompleteResult(list2, null);

        Assert.assertNotEquals(res1, res2);
    }

    @Test
    @SmallTest
    public void autocompleteResult_newItemsAreNotEqual() {
        List<OmniboxSuggestion> list1 =
                Arrays.asList(buildSuggestionForIndex(1), buildSuggestionForIndex(2));
        List<OmniboxSuggestion> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(4));

        AutocompleteResult res1 = new AutocompleteResult(list1, null);
        AutocompleteResult res2 = new AutocompleteResult(list2, null);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_emptyListsAreEqual() {
        final List<OmniboxSuggestion> list1 = new ArrayList<>();
        final List<OmniboxSuggestion> list2 = new ArrayList<>();
        AutocompleteResult res1 = new AutocompleteResult(list1, null);
        AutocompleteResult res2 = new AutocompleteResult(list2, null);
        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_nullAndEmptyListsAreEqual() {
        final List<OmniboxSuggestion> list1 = new ArrayList<>();
        AutocompleteResult res1 = new AutocompleteResult(list1, null);
        AutocompleteResult res2 = new AutocompleteResult(null, null);
        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    @SmallTest
    public void autocompleteResult_emptyAndNonEmptyListsAreNotEqual() {
        List<OmniboxSuggestion> list1 = Arrays.asList(buildSuggestionForIndex(1));
        final List<OmniboxSuggestion> list2 = new ArrayList<>();
        AutocompleteResult res1 = new AutocompleteResult(list1, null);
        AutocompleteResult res2 = new AutocompleteResult(list2, null);
        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }
}
