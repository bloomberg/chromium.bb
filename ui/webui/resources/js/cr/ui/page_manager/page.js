// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.pageManager', function() {
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Base class for pages that can be shown and hidden by PageManager. Each Page
   * is like a node in a forest, corresponding to a particular div. At any
   * point, one root Page is visible, and any visible Page can show a child Page
   * as an overlay. The host of the root Page(s) should provide a container div
   * for each nested level to enforce the stack order of overlays.
   * @constructor
   * @param {string} name Page name.
   * @param {string} title Page title, used for history.
   * @param {string} pageDivName ID of the div corresponding to the page.
   * @extends {cr.EventTarget}
   */
  function Page(name, title, pageDivName) {
    this.name = name;
    this.title = title;
    this.pageDivName = pageDivName;
    this.pageDiv = $(this.pageDivName);
    // |pageDiv.page| is set to the page object (this) when the page is visible
    // to track which page is being shown when multiple pages can share the same
    // underlying div.
    this.pageDiv.page = null;
    this.tab = null;
    this.lastFocusedElement = null;
    this.hash = '';
  }

  Page.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * The parent page of this page, or null for root pages.
     * @type {cr.ui.pageManager.Page}
     */
    parentPage: null,

    /**
     * The section on the parent page that is associated with this page.
     * Can be null.
     * @type {Element}
     */
    associatedSection: null,

    /**
     * An array of controls that are associated with this page. The first
     * control should be located on a root page.
     * @type {Array.<Element>}
     */
    associatedControls: null,

    /**
     * If true, this page should always be considered the top-most page when
     * visible.
     * @type {boolean}
     */
    alwaysOnTop_: false,

    /**
     * Initializes page content.
     */
    initializePage: function() {},

    /**
     * Called by the PageManager when this.hash changes while the page is
     * already visible. This is analogous to the hashchange DOM event.
     */
    didChangeHash: function() {},

    /**
     * Sets focus on the first focusable element. Override for a custom focus
     * strategy.
     */
    focus: function() {
      // Do not change focus if any control on this page is already focused.
      if (this.pageDiv.contains(document.activeElement))
        return;

      var elements = this.pageDiv.querySelectorAll(
          'input, list, select, textarea, button');
      for (var i = 0; i < elements.length; i++) {
        var element = elements[i];
        // Try to focus. If fails, then continue.
        element.focus();
        if (document.activeElement == element)
          return;
      }
    },

    /**
     * Reverses the child elements of this overlay's button strip if it hasn't
     * already been reversed. This is necessary because WebKit does not alter
     * the tab order for elements that are visually reversed using
     * flex-direction: reverse, and the button order is reversed for views.
     * See http://webk.it/62664 for more information.
     */
    reverseButtonStrip: function() {
      assert(this.isOverlay);
      var buttonStrips =
          this.pageDiv.querySelectorAll('.button-strip:not([reversed])');

      // Reverse all button-strips in the overlay.
      for (var j = 0; j < buttonStrips.length; j++) {
        var buttonStrip = buttonStrips[j];

        var childNodes = buttonStrip.childNodes;
        for (var i = childNodes.length - 1; i >= 0; i--)
          buttonStrip.appendChild(childNodes[i]);

        buttonStrip.setAttribute('reversed', '');
      }
    },

    /**
     * Whether it should be possible to show the page.
     * @return {boolean} True if the page should be shown.
     */
    canShowPage: function() {
      return true;
    },

    /**
     * Updates the hash of the current page. If the page is topmost, the history
     * state is updated.
     * @param {string} hash The new hash value. Like location.hash, this
     *     should include the leading '#' if not empty.
     */
    setHash: function(hash) {
      if (this.hash == hash)
        return;
      this.hash = hash;
      PageManager.onPageHashChanged(this);
    },

    /**
     * Called after the page has been shown.
     */
    didShowPage: function() {},

    /**
     * Called before the page will be hidden, e.g., when a different root page
     * will be shown.
     */
    willHidePage: function() {},

    /**
     * Called after the overlay has been closed.
     */
    didClosePage: function() {},

    /**
     * Gets the container div for this page if it is an overlay.
     * @type {HTMLDivElement}
     */
    get container() {
      assert(this.isOverlay);
      return this.pageDiv.parentNode;
    },

    /**
     * Gets page visibility state.
     * @type {boolean}
     */
    get visible() {
      // If this is an overlay dialog it is no longer considered visible while
      // the overlay is fading out. See http://crbug.com/118629.
      if (this.isOverlay &&
          this.container.classList.contains('transparent')) {
        return false;
      }
      if (this.pageDiv.hidden)
        return false;
      return this.pageDiv.page == this;
    },

    /**
     * Sets page visibility.
     * @type {boolean}
     */
    set visible(visible) {
      if ((this.visible && visible) || (!this.visible && !visible))
        return;

      // If using an overlay, the visibility of the dialog is toggled at the
      // same time as the overlay to show the dialog's out transition. This
      // is handled in setOverlayVisible.
      if (this.isOverlay) {
        this.setOverlayVisible_(visible);
      } else {
        this.pageDiv.page = this;
        this.pageDiv.hidden = !visible;
        PageManager.onPageVisibilityChanged(this);
      }

      cr.dispatchPropertyChange(this, 'visible', visible, !visible);
    },

    /**
     * Whether the page is considered 'sticky', such that it will remain a root
     * page even if sub-pages change.
     * @type {boolean} True if this page is sticky.
     */
    get sticky() {
      return false;
    },

    /**
     * @type {boolean} True if this page should always be considered the
     *     top-most page when visible.
     */
    get alwaysOnTop() {
      return this.alwaysOnTop_;
    },

    /**
     * @type {boolean} True if this page should always be considered the
     *     top-most page when visible. Only overlays can be always on top.
     */
    set alwaysOnTop(value) {
      assert(this.isOverlay);
      this.alwaysOnTop_ = value;
    },

    /**
     * Shows or hides an overlay (including any visible dialog).
     * @param {boolean} visible Whether the overlay should be visible or not.
     * @private
     */
    setOverlayVisible_: function(visible) {
      assert(this.isOverlay);
      var pageDiv = this.pageDiv;
      var container = this.container;

      if (container.hidden != visible) {
        if (visible) {
          // If the container is set hidden and then immediately set visible
          // again, the fadeCompleted_ callback would cause it to be erroneously
          // hidden again. Removing the transparent tag avoids that.
          container.classList.remove('transparent');

          // Hide all dialogs in this container since a different one may have
          // been previously visible before fading out.
          var pages = container.querySelectorAll('.page');
          for (var i = 0; i < pages.length; i++)
            pages[i].hidden = true;
          // Show the new dialog.
          pageDiv.hidden = false;
          pageDiv.page = this;
        }
        return;
      }

      var self = this;
      var loading = PageManager.isLoading();
      if (!loading) {
        // TODO(flackr): Use an event delegate to avoid having to subscribe and
        // unsubscribe for webkitTransitionEnd events.
        container.addEventListener('webkitTransitionEnd', function f(e) {
            var propName = e.propertyName;
            if (e.target != e.currentTarget ||
                (propName && propName != 'opacity')) {
              return;
            }
            container.removeEventListener('webkitTransitionEnd', f);
            self.fadeCompleted_();
        });
        // -webkit-transition is 200ms. Let's wait for 400ms.
        ensureTransitionEndEvent(container, 400);
      }

      if (visible) {
        container.hidden = false;
        pageDiv.hidden = false;
        pageDiv.page = this;
        // NOTE: This is a hacky way to force the container to layout which
        // will allow us to trigger the webkit transition.
        /** @suppress {uselessCode} */
        container.scrollTop;

        this.pageDiv.removeAttribute('aria-hidden');
        if (this.parentPage) {
          this.parentPage.pageDiv.parentElement.setAttribute('aria-hidden',
                                                             true);
        }
        container.classList.remove('transparent');
        PageManager.onPageVisibilityChanged(this);
      } else {
        // Kick change events for text fields.
        if (pageDiv.contains(document.activeElement))
          document.activeElement.blur();
        container.classList.add('transparent');
      }

      if (loading)
        this.fadeCompleted_();
    },

    /**
     * Called when a container opacity transition finishes.
     * @private
     */
    fadeCompleted_: function() {
      if (this.container.classList.contains('transparent')) {
        this.pageDiv.hidden = true;
        this.container.hidden = true;

        if (this.parentPage)
          this.parentPage.pageDiv.parentElement.removeAttribute('aria-hidden');

        PageManager.onPageVisibilityChanged(this);
      }
    },
  };

  // Export
  return {
    Page: Page
  };
});
