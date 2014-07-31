// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.pageManager', function() {
  /** @const */ var FocusOutlineManager = cr.ui.FocusOutlineManager;

  /**
   * PageManager contains a list of root Page and overlay Page objects and
   * handles "navigation" by showing and hiding these pages and overlays. On
   * initial load, PageManager can use the path to open the correct hierarchy
   * of pages and overlay(s). Handlers for user events, like pressing buttons,
   * can call into PageManager to open a particular overlay or cancel an
   * existing overlay.
   */
  var PageManager = {
    /**
     * True if page is served from a dialog.
     * @type {boolean}
     */
    isDialog: false,

    /**
     * Offset of page container in pixels, to allow room for side menu.
     * Simplified settings pages can override this if they don't use the menu.
     * The default (155) comes from -webkit-margin-start in uber_shared.css
     * TODO(michaelpg): Remove dependency on uber menu (crbug.com/313244).
     * @type {number}
     */
    horizontalOffset: 155,

    /**
     * Root pages. Maps lower-case page names to the respective page object.
     * @type {!Object.<string, !cr.ui.pageManager.Page>}
     */
    registeredPages: {},

    /**
     * Pages which are meant to behave like modal dialogs. Maps lower-case
     * overlay names to the respective overlay object.
     * @type {!Object.<string, !cr.ui.pageManager.Page>}
     * @private
     */
    registeredOverlayPages: {},

    /**
     * Initializes the complete page.
     * @param {cr.ui.pageManager.Page} defaultPage The page to be shown when no
     *     page is specified in the path.
     */
    initialize: function(defaultPage) {
      this.defaultPage_ = defaultPage;

      FocusOutlineManager.forDocument(document);
      document.addEventListener('scroll', this.handleScroll_.bind(this));

      // Trigger the scroll handler manually to set the initial state.
      this.handleScroll_();

      // Shake the dialog if the user clicks outside the dialog bounds.
      var containers = [$('overlay-container-1'), $('overlay-container-2')];
      for (var i = 0; i < containers.length; i++) {
        var overlay = containers[i];
        cr.ui.overlay.setupOverlay(overlay);
        overlay.addEventListener('cancelOverlay',
                                 this.cancelOverlay.bind(this));
      }

      cr.ui.overlay.globalInitialization();
    },

    /**
     * Registers new page.
     * @param {cr.ui.page_manager.Page} page Page to register.
     */
    register: function(page) {
      this.registeredPages[page.name.toLowerCase()] = page;
      page.initializePage();
    },

    /**
     * Registers a new Overlay page.
     * @param {cr.ui.page_manager.Page} overlay Overlay to register.
     * @param {cr.ui.page_manager.Page} parentPage Associated parent page for
     *     this overlay.
     * @param {Array} associatedControls Array of control elements associated
     *     with this page.
     */
    registerOverlay: function(overlay,
                              parentPage,
                              associatedControls) {
      this.registeredOverlayPages[overlay.name.toLowerCase()] = overlay;
      overlay.parentPage = parentPage;
      if (associatedControls) {
        overlay.associatedControls = associatedControls;
        if (associatedControls.length) {
          overlay.associatedSection =
              this.findSectionForNode_(associatedControls[0]);
        }

        // Sanity check.
        for (var i = 0; i < associatedControls.length; ++i) {
          assert(associatedControls[i], 'Invalid element passed.');
        }
      }

      overlay.tab = undefined;
      overlay.isOverlay = true;

      // Reverse the button strip for Windows and CrOS. See the documentation of
      // cr.ui.pageManager.Page.reverseButtonStrip() for an explanation of why
      // this is done.
      if (cr.isWindows || cr.isChromeOS)
        overlay.reverseButtonStrip();

      overlay.initializePage();
    },

    /**
     * Shows the default page.
     * @param {boolean=} opt_updateHistory If we should update the history after
     *     showing the page (defaults to true).
     */
    showDefaultPage: function(opt_updateHistory) {
      assert(this.defaultPage_ instanceof cr.ui.pageManager.Page,
             'PageManager must be initialized with a default page.');
      this.showPageByName(this.defaultPage_.name, opt_updateHistory);
    },

    /**
     * Shows a registered page. This handles both root and overlay pages.
     * @param {string} pageName Page name.
     * @param {boolean=} opt_updateHistory If we should update the history after
     *     showing the page (defaults to true).
     * @param {Object=} opt_propertyBag An optional bag of properties including
     *     replaceState (if history state should be replaced instead of pushed).
     */
    showPageByName: function(pageName,
                             opt_updateHistory,
                             opt_propertyBag) {
      opt_updateHistory = opt_updateHistory !== false;
      opt_propertyBag = opt_propertyBag || {};

      // If a bubble is currently being shown, hide it.
      this.hideBubble();

      // Find the currently visible root-level page.
      var rootPage = null;
      for (var name in this.registeredPages) {
        var page = this.registeredPages[name];
        if (page.visible && !page.parentPage) {
          rootPage = page;
          break;
        }
      }

      // Find the target page.
      var targetPage = this.registeredPages[pageName.toLowerCase()];
      if (!targetPage || !targetPage.canShowPage()) {
        // If it's not a page, try it as an overlay.
        if (!targetPage && this.showOverlay_(pageName, rootPage)) {
          if (opt_updateHistory)
            this.updateHistoryState_(!!opt_propertyBag.replaceState);
          this.updateTitle_();
          return;
        }
        targetPage = this.defaultPage_;
      }

      pageName = targetPage.name.toLowerCase();
      var targetPageWasVisible = targetPage.visible;

      // Determine if the root page is 'sticky', meaning that it
      // shouldn't change when showing an overlay. This can happen for special
      // pages like Search.
      var isRootPageLocked =
          rootPage && rootPage.sticky && targetPage.parentPage;

      var allPageNames = Array.prototype.concat.call(
          Object.keys(this.registeredPages),
          Object.keys(this.registeredOverlayPages));

      // Notify pages if they will be hidden.
      // TODO(michaelpg): Resolve code duplication.
      for (var i = 0; i < allPageNames.length; ++i) {
        var name = allPageNames[i];
        var page = this.registeredPages[name] ||
                   this.registeredOverlayPages[name];
        if (!page.parentPage && isRootPageLocked)
          continue;
        if (page.willHidePage && name != pageName &&
            !this.isAncestorOfPage(page, targetPage)) {
          page.willHidePage();
        }
      }

      // Update visibilities to show only the hierarchy of the target page.
      for (var i = 0; i < allPageNames.length; ++i) {
        var name = allPageNames[i];
        var page = this.registeredPages[name] ||
                   this.registeredOverlayPages[name];
        if (!page.parentPage && isRootPageLocked)
          continue;
        page.visible = name == pageName ||
                       this.isAncestorOfPage(page, targetPage);
      }

      // Update the history and current location.
      if (opt_updateHistory)
        this.updateHistoryState_(!!opt_propertyBag.replaceState);

      // Update focus if any other control was focused on the previous page,
      // or the previous page is not known.
      if (document.activeElement != document.body &&
          (!rootPage || rootPage.pageDiv.contains(document.activeElement))) {
        targetPage.focus();
      }

      // Notify pages if they were shown.
      for (var i = 0; i < allPageNames.length; ++i) {
        var name = allPageNames[i];
        var page = this.registeredPages[name] ||
                   this.registeredOverlayPages[name];
        if (!page.parentPage && isRootPageLocked)
          continue;
        if (!targetPageWasVisible && page.didShowPage &&
            (name == pageName || this.isAncestorOfPage(page, targetPage))) {
          page.didShowPage();
        }
      }

      // Update the document title. Do this after didShowPage was called, in
      // case a page decides to change its title.
      this.updateTitle_();
    },

    /**
     * Returns the name of the page from the current path.
     * @return {string} Name of the page specified by the current path.
     */
    getPageNameFromPath: function() {
      var path = location.pathname;
      if (path.length <= 1)
        return this.defaultPage_.name;

      // Skip starting slash and remove trailing slash (if any).
      return path.slice(1).replace(/\/$/, '');
    },

    /**
     * Gets the level of the page. The root page (e.g., BrowserOptions) has
     * level 0.
     * @return {number} How far down this page is from the root page.
     */
    getNestingLevel: function(page) {
      if (typeof page.nestingLevelOverride == "number")
        return page.nestingLevelOverride;

      var level = 0;
      var parent = page.parentPage;
      while (parent) {
        level++;
        parent = parent.parentPage;
      }
      return level;
    },

    /**
     * Checks whether one page is an ancestor of the other page in terms of
     * subpage nesting.
     * @param {cr.ui.pageManager.Page} potentialAncestor Potential ancestor.
     * @param {cr.ui.pageManager.Page} potentialDescendent Potential descendent.
     * @return {boolean} True if |potentialDescendent| is nested under
     *     |potentialAncestor|.
     */
    isAncestorOfPage: function(potentialAncestor, potentialDescendent) {
      var parent = potentialDescendent.parentPage;
      while (parent) {
        if (parent == potentialAncestor)
          return true;
        parent = parent.parentPage;
      }
      return false;
    },

    /**
     * Called when an page is shown or hidden to update the root page
     * based on the page's new visibility.
     * @param {cr.ui.pageManager.Page} page The page being made visible or
     *     invisible.
     */
    onPageVisibilityChanged: function(page) {
      this.updateRootPageFreezeState();

      if (page.isOverlay && !page.visible)
        this.updateScrollPosition_();
    },

    /**
     * Returns the topmost visible page, or null if no page is visible.
     * @return {cr.ui.pageManager.Page} The topmost visible page.
     */
    getTopmostVisiblePage: function() {
      // Check overlays first since they're top-most if visible.
      return this.getVisibleOverlay_() ||
          this.getTopmostVisibleNonOverlayPage_();
    },

    /**
     * Closes the visible overlay. Updates the history state after closing the
     * overlay.
     */
    closeOverlay: function() {
      var overlay = this.getVisibleOverlay_();
      if (!overlay)
        return;

      overlay.visible = false;

      if (overlay.didClosePage)
        overlay.didClosePage();
      this.updateHistoryState_(false, {ignoreHash: true});
      this.updateTitle_();

      this.restoreLastFocusedElement_();
    },

    /**
     * Closes all overlays and updates the history after each closed overlay.
     */
    closeAllOverlays: function() {
      while (this.isOverlayVisible_()) {
        this.closeOverlay();
      }
    },

    /**
     * Cancels (closes) the overlay, due to the user pressing <Esc>.
     */
    cancelOverlay: function() {
      // Blur the active element to ensure any changed pref value is saved.
      document.activeElement.blur();
      var overlay = this.getVisibleOverlay_();
      if (!overlay)
        return;
      // Let the overlay handle the <Esc> if it wants to.
      if (overlay.handleCancel) {
        overlay.handleCancel();
        this.restoreLastFocusedElement_();
      } else {
        this.closeOverlay();
      }
    },

    /**
     * Shows an informational bubble displaying |content| and pointing at the
     * |target| element. If |content| has focusable elements, they join the
     * current page's tab order as siblings of |domSibling|.
     * @param {HTMLDivElement} content The content of the bubble.
     * @param {HTMLElement} target The element at which the bubble points.
     * @param {HTMLElement} domSibling The element after which the bubble is
     *     added to the DOM.
     * @param {cr.ui.ArrowLocation} location The arrow location.
     */
    showBubble: function(content, target, domSibling, location) {
      this.hideBubble();

      var bubble = new cr.ui.AutoCloseBubble;
      bubble.anchorNode = target;
      bubble.domSibling = domSibling;
      bubble.arrowLocation = location;
      bubble.content = content;
      bubble.show();
      this.bubble_ = bubble;
    },

    /**
     * Hides the currently visible bubble, if any.
     */
    hideBubble: function() {
      if (this.bubble_)
        this.bubble_.hide();
    },

    /**
     * Returns the currently visible bubble, or null if no bubble is visible.
     * @return {AutoCloseBubble} The bubble currently being shown.
     */
    getVisibleBubble: function() {
      var bubble = this.bubble_;
      return bubble && !bubble.hidden ? bubble : null;
    },

    /**
     * Callback for window.onpopstate to handle back/forward navigations.
     * @param {string} pageName The current page name.
     * @param {Object} data State data pushed into history.
     */
    setState: function(pageName, data) {
      var currentOverlay = this.getVisibleOverlay_();
      var lowercaseName = pageName.toLowerCase();
      var newPage = this.registeredPages[lowercaseName] ||
                    this.registeredOverlayPages[lowercaseName] ||
                    this.defaultPage_;
      if (currentOverlay && !this.isAncestorOfPage(currentOverlay, newPage)) {
        currentOverlay.visible = false;
        if (currentOverlay.didClosePage) currentOverlay.didClosePage();
      }
      this.showPageByName(pageName, false);
    },


    /**
     * Whether the page is still loading (i.e. onload hasn't finished running).
     * @return {boolean} Whether the page is still loading.
     */
    isLoading: function() {
      return document.documentElement.classList.contains('loading');
    },

    /**
     * Callback for window.onbeforeunload. Used to notify overlays that they
     * will be closed.
     */
    willClose: function() {
      var overlay = this.getVisibleOverlay_();
      if (overlay && overlay.didClosePage)
        overlay.didClosePage();
    },

    /**
     * Freezes/unfreezes the scroll position of the root page based on the
     * current page stack.
     */
    updateRootPageFreezeState: function() {
      var topPage = this.getTopmostVisiblePage();
      if (topPage)
        this.setRootPageFrozen_(topPage.isOverlay);
    },

    /**
     * Change the horizontal offset used to reposition elements while showing an
     * overlay from the default.
     */
    setHorizontalOffset: function(value) {
      this.horizontalOffset = value;
    },

    /**
     * Shows a registered overlay page. Does not update history.
     * @param {string} overlayName Page name.
     * @param {cr.ui.pageManager.Page} rootPage The currently visible root-level
     *     page.
     * @return {boolean} Whether we showed an overlay.
     * @private
     */
    showOverlay_: function(overlayName, rootPage) {
      var overlay = this.registeredOverlayPages[overlayName.toLowerCase()];
      if (!overlay || !overlay.canShowPage())
        return false;

      // Save the currently focused element in the page for restoration later.
      var currentPage = this.getTopmostVisiblePage();
      if (currentPage)
        currentPage.lastFocusedElement = document.activeElement;

      if ((!rootPage || !rootPage.sticky) &&
          overlay.parentPage &&
          !overlay.parentPage.visible) {
        this.showPageByName(overlay.parentPage.name, false);
      }

      if (!overlay.visible) {
        overlay.visible = true;
        if (overlay.didShowPage)
          overlay.didShowPage();
      }

      // Change focus to the overlay if any other control was focused by
      // keyboard before. Otherwise, no one should have focus.
      if (document.activeElement != document.body) {
        if (FocusOutlineManager.forDocument(document).visible) {
          overlay.focus();
        } else if (!overlay.pageDiv.contains(document.activeElement)) {
          document.activeElement.blur();
        }
      }

      if ($('search-field') && $('search-field').value == '') {
        var section = overlay.associatedSection;
        if (section)
          options.BrowserOptions.scrollToSection(section);
      }

      return true;
    },

    /**
     * Returns whether or not an overlay is visible.
     * @return {boolean} True if an overlay is visible.
     * @private
     */
    isOverlayVisible_: function() {
      return this.getVisibleOverlay_() != null;
    },

    /**
     * Returns the currently visible overlay, or null if no page is visible.
     * @return {cr.ui.pageManager.Page} The visible overlay.
     * @private
     */
    getVisibleOverlay_: function() {
      var topmostPage = null;
      for (var name in this.registeredOverlayPages) {
        var page = this.registeredOverlayPages[name];
        if (page.visible &&
            (!topmostPage || this.getNestingLevel(page) >
                             this.getNestingLevel(topmostPage))) {
          topmostPage = page;
        }
      }
      return topmostPage;
    },

    /**
     * Hides the visible overlay. Does not affect the history state.
     * @private
     */
    hideOverlay_: function() {
      var overlay = this.getVisibleOverlay_();
      if (overlay)
        overlay.visible = false;
    },

    /**
     * Returns the pages which are currently visible, ordered by nesting level
     * (ascending).
     * @return {Array.Page} The pages which are currently visible, ordered
     * by nesting level (ascending).
     * @private
     */
    getVisiblePages_: function() {
      var visiblePages = [];
      for (var name in this.registeredPages) {
        var page = this.registeredPages[name];
        if (page.visible)
          visiblePages[this.getNestingLevel(page)] = page;
      }
      return visiblePages;
    },

    /**
     * Returns the topmost visible page (overlays excluded).
     * @return {cr.ui.pageManager.Page} The topmost visible page aside from any
     *     overlays.
     * @private
     */
    getTopmostVisibleNonOverlayPage_: function() {
      var topPage = null;
      for (var name in this.registeredPages) {
        var page = this.registeredPages[name];
        if (page.visible &&
            (!topPage || this.getNestingLevel(page) >
                         this.getNestingLevel(topPage))) {
          topPage = page;
        }
      }

      return topPage;
    },

    /**
     * Scrolls the page to the correct position (the top when opening an
     * overlay, or the old scroll position a previously hidden overlay
     * becomes visible).
     * @private
     */
    updateScrollPosition_: function() {
      var container = $('page-container');
      var scrollTop = container.oldScrollTop || 0;
      container.oldScrollTop = undefined;
      window.scroll(scrollLeftForDocument(document), scrollTop);
    },

    /**
     * Updates the title to the title of the current page, or of the topmost
     * visible page with a non-empty title.
     * @private
     */
    updateTitle_: function() {
      var page = this.getTopmostVisiblePage();
      // TODO(michaelpg): Remove dependency on uber (crbug.com/313244).
      while (page) {
        if (page.title) {
          uber.setTitle(page.title);
          return;
        }
        page = page.parentPage;
      }
    },

    /**
     * Pushes the current page onto the history stack, replacing the current
     * entry if appropriate.
     * @param {boolean} replace If true, allow no history events to be created.
     * @param {object=} opt_params A bag of optional params, including:
     *     {boolean} ignoreHash Whether to include the hash or not.
     * @private
     */
    updateHistoryState_: function(replace, opt_params) {
      if (this.isDialog)
        return;

      var page = this.getTopmostVisiblePage();
      var path = window.location.pathname + window.location.hash;
      if (path) {
        // Remove trailing slash.
        path = path.slice(1).replace(/\/(?:#|$)/, '');
      }

      // If the page is already in history (the user may have clicked the same
      // link twice, or this is the initial load), do nothing.
      var hash = opt_params && opt_params.ignoreHash ?
          '' : window.location.hash;
      var newPath = (page == this.defaultPage_ ? '' : page.name) + hash;
      if (path == newPath)
        return;

      // TODO(michaelpg): Remove dependency on uber (crbug.com/313244).
      var historyFunction = replace ? uber.replaceState : uber.pushState;
      historyFunction.call(uber, {}, newPath);
    },

    /**
     * Restores the last focused element on a given page.
     * @private
     */
    restoreLastFocusedElement_: function() {
      var currentPage = this.getTopmostVisiblePage();
      if (currentPage.lastFocusedElement)
        currentPage.lastFocusedElement.focus();
    },

    /**
     * Find an enclosing section for an element if it exists.
     * @param {Element} element Element to search.
     * @return {Element} The section element, or null.
     * @private
     */
    findSectionForNode_: function(node) {
      while (node = node.parentNode) {
        if (node.nodeName == 'SECTION')
          return node;
      }
      return null;
    },

    /**
     * Freezes/unfreezes the scroll position of the root page container.
     * @param {boolean} freeze Whether the page should be frozen.
     * @private
     */
    setRootPageFrozen_: function(freeze) {
      var container = $('page-container');
      if (container.classList.contains('frozen') == freeze)
        return;

      if (freeze) {
        // Lock the width, since auto width computation may change.
        container.style.width = window.getComputedStyle(container).width;
        container.oldScrollTop = scrollTopForDocument(document);
        container.classList.add('frozen');
        var verticalPosition =
            container.getBoundingClientRect().top - container.oldScrollTop;
        container.style.top = verticalPosition + 'px';
        this.updateFrozenElementHorizontalPosition_(container);
      } else {
        container.classList.remove('frozen');
        container.style.top = '';
        container.style.left = '';
        container.style.right = '';
        container.style.width = '';
      }
    },

    /**
     * Called when the page is scrolled; moves elements that are position:fixed
     * but should only behave as if they are fixed for vertical scrolling.
     * @private
     */
    handleScroll_: function() {
      this.updateAllFrozenElementPositions_();
    },

    /**
     * Updates all frozen pages to match the horizontal scroll position.
     * @private
     */
    updateAllFrozenElementPositions_: function() {
      var frozenElements = document.querySelectorAll('.frozen');
      for (var i = 0; i < frozenElements.length; i++)
        this.updateFrozenElementHorizontalPosition_(frozenElements[i]);
    },

    /**
     * Updates the given frozen element to match the horizontal scroll position.
     * @param {HTMLElement} e The frozen element to update.
     * @private
     */
    updateFrozenElementHorizontalPosition_: function(e) {
      if (isRTL()) {
        e.style.right = this.horizontalOffset + 'px';
      } else {
        var scrollLeft = scrollLeftForDocument(document);
        e.style.left = this.horizontalOffset - scrollLeft + 'px';
      }
    },
  };

  // Export
  return {
    PageManager: PageManager
  };
});
