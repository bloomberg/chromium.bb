// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_test', function() {

  suite('SearchSettingsTest', function() {
    let searchManager;

    // Don't import script if already imported (happens in Vulcanized mode).
    suiteSetup(function() {
      if (!window.settings || !settings.getSearchManager) {
        return PolymerTest.loadScript(
                   'chrome://resources/js/search_highlight_utils.js') &&
            PolymerTest.loadScript('chrome://settings/search_settings.js');
      }
    });

    setup(function() {
      searchManager = settings.getSearchManager();
      PolymerTest.clearBody();
    });

    /**
     * Test that the DOM of a node is modified as expected when a search hit
     * occurs within that node.
     */
    test('normal highlighting', function() {
      const optionText = 'FooSettingsFoo';

      document.body.innerHTML = `<settings-section hidden-by-search>
             <div id="mydiv">${optionText}</div>
           </settings-section>`;

      const section = document.querySelector('settings-section');
      const div = document.querySelector('#mydiv');

      assertTrue(section.hiddenBySearch);
      return searchManager.search('settings', section)
          .then(function() {
            assertFalse(section.hiddenBySearch);

            const highlightWrapper =
                div.querySelector('.search-highlight-wrapper');
            assertTrue(!!highlightWrapper);

            const originalContent = highlightWrapper.querySelector(
                '.search-highlight-original-content');
            assertTrue(!!originalContent);
            assertEquals(optionText, originalContent.textContent);

            const searchHits =
                highlightWrapper.querySelectorAll('.search-highlight-hit');
            assertEquals(1, searchHits.length);
            assertEquals('Settings', searchHits[0].textContent);

            // Check that original DOM structure is restored when search
            // highlights are cleared.
            return searchManager.search('', section);
          })
          .then(function() {
            assertEquals(0, div.children.length);
            assertEquals(optionText, div.textContent);
          });
    });

    /**
     * Tests that a search hit within a <select> node causes the parent
     * settings-section to be shown, but the DOM of the <select> is not
     * modified.
     */
    test('<select> highlighting', function() {
      document.body.innerHTML = `<settings-section hidden-by-search>
             <select>
               <option>Foo</option>
               <option>Settings</option>
               <option>Baz</option>
             </select>
           </settings-section>`;

      const section = document.querySelector('settings-section');
      const select = section.querySelector('select');

      assertTrue(section.hiddenBySearch);
      return searchManager.search('settings', section)
          .then(function() {
            assertFalse(section.hiddenBySearch);

            const highlightWrapper =
                select.querySelector('.search-highlight-wrapper');
            assertFalse(!!highlightWrapper);

            // Check that original DOM structure is present even after search
            // highlights are cleared.
            return searchManager.search('', section);
          })
          .then(function() {
            const options = select.querySelectorAll('option');
            assertEquals(3, options.length);
            assertEquals('Foo', options[0].textContent);
            assertEquals('Settings', options[1].textContent);
            assertEquals('Baz', options[2].textContent);
          });
    });

    test('ignored elements are ignored', function() {
      const text = 'hello';
      document.body.innerHTML = `<settings-section hidden-by-search>
             <cr-action-menu>${text}</cr-action-menu>
             <cr-dialog>${text}</cr-dialog>
             <cr-icon-button>${text}</cr-icon-button>
             <cr-slider>${text}</cr-slider>
             <dialog>${text}</dialog>
             <iron-icon>${text}</iron-icon>
             <iron-list>${text}</iron-list>
             <paper-ripple>${text}</paper-ripple>
             <paper-spinner-lite>${text}</paper-spinner-lite>
             <slot>${text}</slot>
             <content>${text}</content>
             <style>${text}</style>
             <template>${text}</template>
           </settings-section>`;

      const section = document.querySelector('settings-section');
      assertTrue(section.hiddenBySearch);

      return searchManager.search(text, section).then(function() {
        assertTrue(section.hiddenBySearch);
      });
    });

    test('no-search elements are ignored', function() {
      // Define a dummy test element with the necessary structure for testing.
      Polymer({
        is: 'dummy-test-element',

        properties: {
          noSearch: {
            type: Boolean,
            value: true,
          },
        },
      });

      const text = 'hello';

      document.body.innerHTML = `
        <dom-module id="dummy-test-element">
          <template>
            <button></button>
            <settings-section hidden-by-search>
              <!-- Test case were no-search is part of a data binding. -->
              <template is="dom-if" route-path="/myPath0"
                  no-search="[[noSearch]]">
                <settings-subpage associated-control="[[$$('button')]]">
                  ${text}
                </settings-subpage>
              </template>

              <!-- Test case were no-search is not part of any data binding.-->
              <template is="dom-if" route-path="/myPath1" no-search>
                <settings-subpage associated-control="[[$$('button')]]">
                  ${text}
                </settings-subpage>
              </template>
            </settings-section>
          </template>
        </dom-module>
        <dummy-test-element></dummy-test-element>`;

      const element = document.body.querySelector('dummy-test-element');
      const section = element.$$('settings-section');

      // Ensure that no settings-subpage instance exists.
      assertEquals(null, element.$$('settings-subpage'));

      return searchManager.search(text, section)
          .then(function() {
            assertTrue(section.hiddenBySearch);
            // Check that searching did not cause a settings-subpage instance to
            // be forced rendered.
            assertEquals(null, element.$$('settings-subpage'));

            element.noSearch = false;
            return searchManager.search(text, section);
          })
          .then(function() {
            // Check that searching caused a settings-subpage instance to be
            // forced rendered.
            assertFalse(section.hiddenBySearch);
            assertTrue(!!element.$$('settings-subpage'));
          });
    });

    // Test that multiple requests for the same text correctly highlight their
    // corresponding part of the tree without affecting other parts of the tree.
    test('multiple simultaneous requests for the same text', function() {
      document.body.innerHTML = `<settings-section hidden-by-search>
             <div><span>Hello there</span></div>
           </settings-section>
           <settings-section hidden-by-search>
             <div><span>Hello over there</span></div>
           </settings-section>
           <settings-section hidden-by-search>
             <div><span>Nothing</span></div>
           </settings-section>`;

      const sections = Array.prototype.slice.call(
          document.querySelectorAll('settings-section'));

      return Promise.all(sections.map(s => searchManager.search('there', s)))
          .then(function(requests) {
            assertTrue(requests[0].didFindMatches());
            assertFalse(sections[0].hiddenBySearch);
            assertTrue(requests[1].didFindMatches());
            assertFalse(sections[1].hiddenBySearch);
            assertFalse(requests[2].didFindMatches());
            assertTrue(sections[2].hiddenBySearch);
          });
    });

    test('highlight removed when text is changed', function() {
      const originalText = 'FooSettingsFoo';

      document.body.innerHTML = `<settings-section hidden-by-search>
            <div id="mydiv">${originalText}</div>
          </settings-section>`;

      const section = document.querySelector('settings-section');
      const div = document.querySelector('#mydiv');
      assertTrue(section.hiddenBySearch);
      return searchManager.search('settings', document.body).then(() => {
        assertFalse(section.hiddenBySearch);
        assertEquals(1, div.childNodes.length);
        const highlightWrapper = div.firstChild;
        assertTrue(
            highlightWrapper.classList.contains('search-highlight-wrapper'));
        const originalContent = highlightWrapper.querySelector(
            '.search-highlight-original-content');
        assertTrue(!!originalContent);
        originalContent.childNodes[0].nodeValue = 'Foo';
        return new Promise(resolve => {
          setTimeout(() => {
            assertFalse(section.hiddenBySearch);
            assertEquals(1, div.childNodes.length);
            assertEquals('Foo', div.innerHTML);
            resolve();
          }, 1);
        });
      });
    });

    test('match text outside of a settings section', async function() {
      document.body.innerHTML = `
          <div id="mydiv">Match</div>
          <settings-section></settings-section>`;

      const section = document.querySelector('settings-section');
      const mydiv = document.querySelector('#mydiv');

      await searchManager.search('Match', document.body);
      assertTrue(section.hiddenBySearch);

      const highlight = mydiv.querySelector('.search-highlight-wrapper');
      assertTrue(!!highlight);

      const searchHits = highlight.querySelectorAll('.search-highlight-hit');
      assertEquals(1, searchHits.length);
      assertEquals('Match', searchHits[0].textContent);
    });

    test('associated control causes search highlight bubble', async () => {
      document.body.innerHTML = `
          <settings-section>
            <button></button>
            <settings-subpage>
              hello
            </settings-subpage>
          </settings-section>`;
      const subpage = document.querySelector('settings-subpage');
      subpage.associatedControl = document.querySelector('button');

      await searchManager.search('hello', document.body);

      assertEquals(1, document.querySelectorAll('.search-bubble').length);
    });
  });
});
