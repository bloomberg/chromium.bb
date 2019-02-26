// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('nuxEmail');

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 *   bookmarkId: (string|undefined|null),
 * }}
 */
nuxEmail.EmailProviderModel;

Polymer({
  is: 'email-chooser',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {!Array<!nux.BookmarkListItem>}
     * @private
     */
    emailList_: Array,

    /** @private */
    finalized_: Boolean,

    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,

    /** @private {?nuxEmail.EmailProviderModel} */
    selectedEmailProvider_: {
      type: Object,
      observer: 'onSelectedEmailProviderChange_',
    },
  },

  /** @private {nux.NuxEmailProxy} */
  emailProxy_: null,

  /** @private {nux.BookmarkProxy} */
  bookmarkProxy_: null,

  /** @private {nux.BookmarkBarManager} */
  bookmarkBarManager_: null,

  /** @private {boolean} */
  wasBookmarkBarShownOnInit_: false,

  /** @private {Promise} */
  listInitialized_: null,

  /** @private {?nux.ModuleMetricsManager} */
  metricsManager_: null,

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  ready: function() {
    this.emailProxy_ = nux.NuxEmailProxyImpl.getInstance();
    this.bookmarkProxy_ = nux.BookmarkProxyImpl.getInstance();
    this.bookmarkBarManager_ = nux.BookmarkBarManager.getInstance();
    this.metricsManager_ =
        new nux.ModuleMetricsManager(nux.EmailMetricsProxyImpl.getInstance());

    this.listInitialized_ = this.emailProxy_.getEmailList().then(list => {
      this.emailList_ = list;
    });
  },

  onRouteEnter: function() {
    this.wasBookmarkBarShownOnInit_ = this.bookmarkBarManager_.getShown();
    this.metricsManager_.recordPageInitialized();
    this.finalized_ = false;

    assert(this.listInitialized_);
    this.listInitialized_.then(() => {
      // If selectedEmailProvider_ was never initialized, and not explicitly
      // cancelled by the user at some point (in which case it would be null),
      // then default to the first option.
      if (this.selectedEmailProvider_ === undefined) {
        this.selectedEmailProvider_ = this.emailList_[0];
      }

      if (this.selectedEmailProvider_) {
        this.addBookmark_(this.selectedEmailProvider_);
      }
    });
  },

  onRouteExit: function() {
    if (this.finalized_)
      return;
    this.cleanUp_();
    this.metricsManager_.recordBrowserBackOrForward();
  },

  onRouteUnload: function() {
    if (this.finalized_)
      return;
    this.cleanUp_();
    this.metricsManager_.recordNavigatedAway();
  },

  /**
   * Removes any bookarks and hides the bookmark bar when finalizing.
   * @private
   */
  cleanUp_: function() {
    this.finalized_ = true;
    if (this.selectedEmailProvider_) {
      this.revertBookmark_();
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);
    }
  },

  /**
   * Handle toggling the email selected.
   * @param {!{model: {item: !nuxEmail.EmailProviderModel}}} e
   * @private
   */
  onEmailClick_: function(e) {
    if (this.getSelected_(e.model.item))
      this.selectedEmailProvider_ = null;
    else
      this.selectedEmailProvider_ = e.model.item;

    this.metricsManager_.recordClickedOption();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEmailPointerDown_: function(e) {
    e.currentTarget.classList.remove('keyboard-focused');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEmailKeyUp_: function(e) {
    e.currentTarget.classList.add('keyboard-focused');
  },

  /**
   * Returns whether |item| is selected or not.
   * @param {!nuxEmail.EmailProviderModel} item
   * @return boolean
   * @private
   */
  getSelected_: function(item) {
    return this.selectedEmailProvider_ &&
        item.name === this.selectedEmailProvider_.name;
  },

  /**
   * @param {nuxEmail.EmailProviderModel} emailProvider
   * @private
   */
  addBookmark_: function(emailProvider) {
    if (emailProvider.bookmarkId)
      return;

    // Indicates that the emailProvider is being added as a bookmark.
    emailProvider.bookmarkId = 'pending';

    this.emailProxy_.cacheBookmarkIcon(emailProvider.id);
    this.bookmarkBarManager_.setShown(true);
    this.bookmarkProxy_.addBookmark(
        {
          title: emailProvider.name,
          url: emailProvider.url,
          parentId: '1',
        },
        results => {
          this.selectedEmailProvider_.bookmarkId = results.id;
        });
  },

  /**
   * @param {nuxEmail.EmailProviderModel=} opt_emailProvider
   * @private
   */
  revertBookmark_: function(opt_emailProvider) {
    const emailProvider = opt_emailProvider || this.selectedEmailProvider_;

    if (emailProvider && emailProvider.bookmarkId) {
      this.bookmarkProxy_.removeBookmark(emailProvider.bookmarkId);
      emailProvider.bookmarkId = null;
    }
  },

  /**
   * @param {nuxEmail.EmailProviderModel} newEmail
   * @param {nuxEmail.EmailProviderModel} prevEmail
   * @private
   */
  onSelectedEmailProviderChange_: function(newEmail, prevEmail) {
    if (!this.emailProxy_ || !this.bookmarkProxy_)
      return;

    if (prevEmail) {
      // If it was previously selected, it must've been assigned an id.
      assert(prevEmail.bookmarkId);
      this.revertBookmark_(prevEmail);
    }

    if (newEmail)
      this.addBookmark_(newEmail);
    else
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);

    // Announcements are mutually exclusive, so keeping separate.
    if (prevEmail && newEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkReplaced')});
    } else if (prevEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkRemoved')});
    } else if (newEmail) {
      this.fire('iron-announce', {text: this.i18n('bookmarkAdded')});
    }
  },

  /** @private */
  onNoThanksClicked_: function() {
    this.cleanUp_();
    this.metricsManager_.recordNoThanks();
    welcome.navigateToNextStep();
  },

  /** @private */
  onGetStartedClicked_: function() {
    this.finalized_ = true;
    this.emailProxy_.recordProviderSelected(
        this.selectedEmailProvider_.id, this.emailList_.length);
    this.metricsManager_.recordGetStarted();
    welcome.navigateToNextStep();
  },

  /** @private */
  onActionButtonClicked_: function() {
    if (this.$$('.action-button').disabled)
      this.metricsManager_.recordClickedDisabledButton();
  },

  /**
   * Converts a boolean to a string because aria-pressed needs a string value.
   * @param {!nuxEmail.EmailProviderModel} item
   * @return {string}
   * @private
   */
  getAriaPressed_: function(item) {
    return this.getSelected_(item) ? 'true' : 'false';
  }
});
