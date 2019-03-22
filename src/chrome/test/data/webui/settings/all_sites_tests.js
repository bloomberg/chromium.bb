// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('AllSites', function() {
  const TEST_COOKIE_LIST = {
    id: 'example',
    children: [
      {domain: 'bar.com'},
      {domain: 'bar.com'},
      {domain: 'bar.com'},
      {domain: 'bar.com'},
      {domain: 'google.com'},
      {domain: 'google.com'},
    ]
  };

  /**
   * An example pref with multiple categories and multiple allow/block
   * state.
   * @type {SiteSettingsPref}
   */
  let prefsVarious;

  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  /**
   * The mock local data proxy object to use during test.
   * @type {TestLocalDataBrowserProxy}
   */
  let localDataBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(function() {
    prefsVarious = test_util.createSiteSettingsPrefs([], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.GEOLOCATION,
          [
            test_util.createRawSiteException('https://foo.com'),
            test_util.createRawSiteException('https://bar.com', {
              setting: settings.ContentSetting.BLOCK,
            })
          ]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          [
            test_util.createRawSiteException('https://google.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('https://bar.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('https://foo.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
          ])
    ]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    settings.LocalDataBrowserProxyImpl.instance_ = localDataBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('all-sites');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();
  });

  /**
   * Configures the test element.
   * @param {Array<dictionary>} prefs The prefs to use.
   */
  function setUpCategory(prefs) {
    browserProxy.setPrefs(prefs);
    settings.navigateTo(settings.routes.SITE_SETTINGS_ALL);
  }

  test('All sites list populated', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites').then(() => {
      // Use resolver to ensure that the list container is populated.
      const resolver = new PromiseResolver();
      // In Polymer2, we need to wait until after the next render for the list
      // to be populated. TODO (rbpotter): Remove conditional when migration to
      // Polymer 2 is completed.
      if (Polymer.DomIf) {
        Polymer.RenderStatus.beforeNextRender(testElement, () => {
          resolver.resolve();
        });
      } else {
        testElement.async(resolver.resolve);
      }
      return resolver.promise.then(() => {
        assertEquals(3, testElement.siteGroupMap.size);

        // Flush to be sure list container is populated.
        Polymer.dom.flush();
        const siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(3, siteEntries.length);
      });
    });
  });

  test('search query filters list', function() {
    const SEARCH_QUERY = 'foo';
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites')
        .then(() => {
          // Flush to be sure list container is populated.
          Polymer.dom.flush();
          const siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          assertEquals(3, siteEntries.length);

          testElement.searchQuery_ = SEARCH_QUERY;
        })
        .then(() => {
          Polymer.dom.flush();
          const siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          const hiddenSiteEntries = Polymer.dom(testElement.root)
                                        .querySelectorAll('site-entry[hidden]');
          assertEquals(1, siteEntries.length - hiddenSiteEntries.length);

          for (let i = 0; i < siteEntries; ++i) {
            const entry = siteEntries[i];
            if (!hiddenSiteEntries.includes(entry)) {
              assertTrue(entry.siteGroup.origins.some((origin) => {
                return origin.includes(SEARCH_QUERY);
              }));
            }
          }
        });
  });

  test('can be sorted by most visited', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();

    return browserProxy.whenCalled('getAllSites').then(() => {
      // Add additional origins and artificially insert fake engagement scores
      // to sort.
      assertEquals(3, testElement.siteGroupMap.size);
      const fooSiteGroup = testElement.siteGroupMap.get('foo.com');
      fooSiteGroup.origins.push(test_util.createOriginInfo(
          'https://login.foo.com', {engagement: 20}));
      assertEquals(2, fooSiteGroup.origins.length);
      fooSiteGroup.origins[0].engagement = 50.4;
      const googleSiteGroup = testElement.siteGroupMap.get('google.com');
      assertEquals(1, googleSiteGroup.origins.length);
      googleSiteGroup.origins[0].engagement = 55.1261;
      const barSiteGroup = testElement.siteGroupMap.get('bar.com');
      assertEquals(1, barSiteGroup.origins.length);
      barSiteGroup.origins[0].engagement = 0.5235;

      // 'Most visited' is the default sort method, so sort by a different
      // method first to ensure changing to 'Most visited' works.
      testElement.root.querySelector('select').value = 'name';
      testElement.onSortMethodChanged_();
      Polymer.dom.flush();
      let siteEntries =
          testElement.$.listContainer.querySelectorAll('site-entry');
      assertEquals('bar.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());

      testElement.root.querySelector('select').value = 'most-visited';
      testElement.onSortMethodChanged_();
      Polymer.dom.flush();
      siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
      // Each site entry is sorted by its maximum engagement, so expect
      // 'foo.com' to come after 'google.com'.
      assertEquals('google.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('bar.com', siteEntries[2].$.displayName.innerText.trim());
    });
  });

  test('can be sorted by storage', function() {
    localDataBrowserProxy.setCookieDetails(TEST_COOKIE_LIST);
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites')
        .then(() => {
          Polymer.dom.flush();
          let siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          // Add additional origins to SiteGroups with cookies to simulate their
          // being grouped entries, plus add local storage.
          siteEntries[0].siteGroup.origins[0].usage = 1000;
          siteEntries[1].siteGroup.origins.push(
              test_util.createOriginInfo('http://bar.com'));
          siteEntries[1].siteGroup.origins[0].usage = 500;
          siteEntries[1].siteGroup.origins[1].usage = 500;
          siteEntries[2].siteGroup.origins.push(
              test_util.createOriginInfo('http://google.com'));

          testElement.onSortMethodChanged_();
          siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          // Verify all sites is not sorted by storage.
          assertEquals(3, siteEntries.length);
          assertEquals(
              'foo.com', siteEntries[0].$.displayName.innerText.trim());
          assertEquals(
              'bar.com', siteEntries[1].$.displayName.innerText.trim());
          assertEquals(
              'google.com', siteEntries[2].$.displayName.innerText.trim());

          // Change the sort method, then verify all sites is now sorted by
          // name.
          testElement.root.querySelector('select').value = 'data-stored';
          testElement.onSortMethodChanged_();
          return localDataBrowserProxy.whenCalled('getNumCookiesList');
        })
        .then(() => {
          Polymer.dom.flush();
          let siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          assertEquals(
              'bar.com',
              siteEntries[0]
                  .root.querySelector('#displayName .url-directionality')
                  .innerText.trim());
          assertEquals(
              'foo.com',
              siteEntries[1]
                  .root.querySelector('#displayName .url-directionality')
                  .innerText.trim());
          assertEquals(
              'google.com',
              siteEntries[2]
                  .root.querySelector('#displayName .url-directionality')
                  .innerText.trim());
        });
  });

  test('can be sorted by name', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites').then(() => {
      Polymer.dom.flush();
      let siteEntries =
          testElement.$.listContainer.querySelectorAll('site-entry');

      // Verify all sites is not sorted by name.
      assertEquals(3, siteEntries.length);
      assertEquals('foo.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('bar.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());

      // Change the sort method, then verify all sites is now sorted by name.
      testElement.root.querySelector('select').value = 'name';
      testElement.onSortMethodChanged_();
      Polymer.dom.flush();
      siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
      assertEquals('bar.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());
    });
  });

  test('merging additional SiteGroup lists works', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites').then(() => {
      Polymer.dom.flush();
      let siteEntries =
          testElement.$.listContainer.querySelectorAll('site-entry');
      assertEquals(3, siteEntries.length);

      // Pretend an additional set of SiteGroups were added.
      const fooEtldPlus1 = 'foo.com';
      const addEtldPlus1 = 'additional-site.net';
      const fooOrigin = 'https://login.foo.com';
      const addOrigin = 'http://www.additional-site.net';
      const LOCAL_STORAGE_SITE_GROUP_LIST = /** @type {!Array{!SiteGroup}} */ ([
        {
          // Test merging an existing site works, with overlapping origin lists.
          'etldPlus1': fooEtldPlus1,
          'origins': [
            test_util.createOriginInfo(fooOrigin),
            test_util.createOriginInfo('https://foo.com'),
          ],
        },
        {
          // Test adding a new site entry works.
          'etldPlus1': addEtldPlus1,
          'origins': [test_util.createOriginInfo(addOrigin)],
        }
      ]);
      testElement.onLocalStorageListFetched(LOCAL_STORAGE_SITE_GROUP_LIST);

      Polymer.dom.flush();
      siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
      assertEquals(4, siteEntries.length);

      assertEquals(fooEtldPlus1, siteEntries[0].siteGroup.etldPlus1);
      assertEquals(2, siteEntries[0].siteGroup.origins.length);
      assertEquals(
          'https://foo.com', siteEntries[0].siteGroup.origins[0].origin);
      assertEquals(fooOrigin, siteEntries[0].siteGroup.origins[1].origin);

      assertEquals(addEtldPlus1, siteEntries[3].siteGroup.etldPlus1);
      assertEquals(1, siteEntries[3].siteGroup.origins.length);
      assertEquals(addOrigin, siteEntries[3].siteGroup.origins[0].origin);
    });
  });
});
