// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ExploreSitesCategory} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExploreSitesCategoryUnitTest {
    @Test
    public void testAddSite() {
        final int id = 1;
        @ExploreSitesCategory.CategoryType
        final int type = ExploreSitesCategory.CategoryType.SCIENCE;
        final int siteId = 100;
        final String title = "test";
        final String url = "http://www.google.com";
        final String categoryTitle = "Movies";

        ExploreSitesCategory category = new ExploreSitesCategory(id, type, categoryTitle);
        category.addSite(new ExploreSitesSite(siteId, title, url, true)); // blacklisted
        category.addSite(new ExploreSitesSite(siteId, title, url, false)); // not blacklisted

        assertEquals(id, category.getId());
        assertEquals(type, category.getType());
        assertEquals(2, category.getSites().size());
        assertEquals(1, category.getNumDisplayed());
        assertEquals(siteId, category.getSites().get(0).getModel().get(ExploreSitesSite.ID_KEY));
        assertEquals(title, category.getSites().get(0).getModel().get(ExploreSitesSite.TITLE_KEY));
        assertEquals(url, category.getSites().get(0).getModel().get(ExploreSitesSite.URL_KEY));
        assertTrue(category.getSites().get(0).getModel().get(ExploreSitesSite.BLACKLISTED_KEY));
        assertFalse(category.getSites().get(1).getModel().get(ExploreSitesSite.BLACKLISTED_KEY));
    }
}
