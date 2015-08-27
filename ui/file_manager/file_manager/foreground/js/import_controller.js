// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/** @private @enum {string} */
importer.ActivityState = {
  READY: 'ready',
  HIDDEN: 'hidden',
  IMPORTING: 'importing',
  INSUFFICIENT_SPACE: 'insufficient-space',
  NO_MEDIA: 'no-media',
  SCANNING: 'scanning'
};

/**
 * Class that orchestrates background activity and UI changes on
 * behalf of Cloud Import.
 *
 * @constructor
 * @struct
 *
 * @param {!importer.ControllerEnvironment} environment The class providing
 *     access to runtime environmental information, like the current directory,
 *     volume lookup and so-on.
 * @param {!importer.MediaScanner} scanner
 * @param {!importer.ImportRunner} importRunner
 * @param {!importer.CommandWidget} commandWidget
 * @param {!analytics.Tracker} tracker
 */
importer.ImportController =
    function(environment, scanner, importRunner, commandWidget, tracker) {

  /** @private {!importer.ControllerEnvironment} */
  this.environment_ = environment;

  /** @private {!importer.ChromeLocalStorage} */
  this.storage_ = importer.ChromeLocalStorage.getInstance();

  /** @private {!importer.ImportRunner} */
  this.importRunner_ = importRunner;

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /** @private {!importer.CommandWidget} */
  this.commandWidget_ = commandWidget;

  /** @type {!importer.ScanManager} */
  this.scanManager_ = new importer.ScanManager(environment, scanner);

  /** @private {!analytics.Tracker} */
  this.tracker_ = tracker;

  /**
   * The active import task, if any.
   * @private {?importer.TaskDetails_}
   */
  this.activeImport_ = null;

  /**
   * The previous import task, if any.
   * @private {?importer.TaskDetails_}
   */
  this.previousImport_ = null;

  /**
   * @private {!importer.ActivityState}
   */
  this.lastActivityState_ = importer.ActivityState.HIDDEN;

  var listener = this.onScanEvent_.bind(this);
  this.scanner_.addObserver(listener);
  // Remove the observer when the foreground window is closed.
  window.addEventListener(
      'pagehide',
      function() {
        this.scanner_.removeObserver(listener);
      }.bind(this));

  this.environment_.addWindowCloseListener(
      this.onWindowClosing_.bind(this));

  this.environment_.addVolumeUnmountListener(
      this.onVolumeUnmounted_.bind(this));

  this.environment_.addDirectoryChangedListener(
      this.onDirectoryChanged_.bind(this));

  this.environment_.addSelectionChangedListener(
      this.onSelectionChanged_.bind(this));

  this.commandWidget_.addClickListener(
      this.onClick_.bind(this));

  this.storage_.get(importer.Setting.HAS_COMPLETED_IMPORT, false)
      .then(
          /**
           * @param {boolean} importCompleted If so, we hide the banner
           * @this {importer.ImportController}
           */
          function(importCompleted) {
            this.commandWidget_.setDetailsBannerVisible(!importCompleted);
          }.bind(this));
};

/**
 * Collection of import task related details.
 *
 * @typedef {{
 *   scan: !importer.ScanResult,
 *   task: !importer.MediaImportHandler.ImportTask,
 *   started: !Date
 * }}
 * @private
 */
importer.TaskDetails_;

/**
 * @param {!importer.ScanEvent} event Command event.
 * @param {importer.ScanResult} scan
 *
 * @private
 */
importer.ImportController.prototype.onScanEvent_ = function(event, scan) {
  if (!this.scanManager_.isActiveScan(scan)) {
    return;
  }

  switch (event) {
    case importer.ScanEvent.INVALIDATED:
      this.onScanInvalidated_();
    case importer.ScanEvent.FINALIZED:
    case importer.ScanEvent.UPDATED:
      this.checkState_(scan);
      break;
  }
};

/** @private */
importer.ImportController.prototype.onWindowClosing_ = function() {
  this.scanManager_.reset();  // Will cancel any active scans.
};

/**
 * @param {string} volumeId
 * @private
 */
importer.ImportController.prototype.onVolumeUnmounted_ = function(volumeId) {
  if (this.activeImport_) {
    this.activeImport_.task.requestCancel();
    this.finalizeActiveImport_();
    this.tracker_.send(metrics.ImportEvents.DEVICE_YANKED);
  }
  this.scanManager_.reset();
  this.checkState_();
};

/**
 * @param {!Event} event
 * @private
 */
importer.ImportController.prototype.onDirectoryChanged_ = function(event) {
  if (!event.previousDirEntry &&
      event.newDirEntry &&
      importer.isMediaDirectory(event.newDirEntry, this.environment_)) {
    this.commandWidget_.setDetailsVisible(true);
  }

  this.scanManager_.reset();
  if (this.isCurrentDirectoryScannable_()) {
    this.checkState_(
        this.scanManager_.getDirectoryScan());
  } else {
    this.checkState_();
  }
};

/** @private */
importer.ImportController.prototype.onSelectionChanged_ = function() {
  // NOTE: Empty selection changed events can and will fire for a directory
  // before we receive the the corresponding directory changed event
  // and when the selection is empty. These are spurios events
  // and we ignore them.
  if (!this.scanManager_.hasSelectionScan() &&
      this.environment_.getSelection().length === 0) {
    return;
  }

  // NOTE: We clear the scan here, but don't immediately initiate
  // a new scan. checkState will attempt to initialize the scan
  // during normal processing.
  // Also, in the case the selection is transitioning to empty,
  // we want to reinstate the underlying directory scan (if
  // in fact, one is possible).
  this.scanManager_.clearSelectionScan();
  if (this.environment_.getSelection().length === 0 &&
      this.isCurrentDirectoryScannable_()) {
    this.checkState_(
        this.scanManager_.getDirectoryScan());
  } else {
    this.checkState_();
  }
};

/**
 * @param {!importer.MediaImportHandler.ImportTask} task
 * @private
 */
importer.ImportController.prototype.onImportFinished_ = function(task) {
  this.finalizeActiveImport_();
  this.scanManager_.reset();
  this.storage_.set(importer.Setting.HAS_COMPLETED_IMPORT, true);
  this.commandWidget_.setDetailsBannerVisible(false);
  this.checkState_();
};

/** @private */
importer.ImportController.prototype.onScanInvalidated_ = function() {
  this.scanManager_.reset();
  if (this.environment_.getSelection().length === 0 &&
      this.isCurrentDirectoryScannable_()) {
    this.checkState_(
        this.scanManager_.getDirectoryScan());
  } else {
    this.checkState_();
  }
};

/**
 * Does book keeping necessary to finalize the active task.
 * @private
 */
importer.ImportController.prototype.finalizeActiveImport_ = function() {
  console.assert(!!this.activeImport_,
      'Cannot finish import when none is running.');
  this.previousImport_ = this.activeImport_;
  this.activeImport_ = null;
};

/**
 * Handles button clicks emenating from the panel or toolbar.
 * @param {!importer.ClickSource} source
 */
importer.ImportController.prototype.onClick_ =
     function(source) {
  switch (source) {
    case importer.ClickSource.MAIN:
      if (this.lastActivityState_ === importer.ActivityState.READY) {
        this.commandWidget_.performMainButtonRippleAnimation();
        this.execute_();
      } else {
        this.commandWidget_.toggleDetails();
      }
      break;
    case importer.ClickSource.IMPORT:
      this.execute_();
      break;
    case importer.ClickSource.CANCEL:
      this.cancel_();
      break;
    case importer.ClickSource.DESTINATION:
      if (this.activeImport_) {
        this.environment_.showImportDestination(this.activeImport_.started);
      } else if (this.previousImport_) {
        this.environment_.showImportDestination(this.previousImport_.started);
      } else {
        this.environment_.showImportRoot();
      }
      break;
    case importer.ClickSource.SIDE:
      // Intentionally unhandled...panel controls toggling of details panel.
      break;
    default:
      assertNotReached('Unhandled click source: ' + source);
  }
};

/**
 * Executes import against the current directory. Should only
 * be called when the current directory has been validated
 * by calling "update" on this class.
 *
 * @private
 */
importer.ImportController.prototype.execute_ = function() {
  console.assert(!this.activeImport_,
      'Cannot execute while an import task is already active.');

  var scan = this.scanManager_.getActiveScan();
  assert(scan != null);

  var startDate = new Date();
  var importTask = this.importRunner_.importFromScanResult(
      scan,
      importer.Destination.GOOGLE_DRIVE,
      this.environment_.getImportDestination(startDate));

  this.activeImport_ = {
    scan: scan,
    task: importTask,
    started: startDate
  };
  var taskFinished = this.onImportFinished_.bind(this, importTask);
  importTask.whenFinished.then(taskFinished).catch(taskFinished);
  this.checkState_();
};

/**
 * Cancels the active task.
 * @private
 */
importer.ImportController.prototype.cancel_ = function() {
  if (this.activeImport_) {
    this.activeImport_.task.requestCancel();
    this.finalizeActiveImport_();
    this.tracker_.send(metrics.ImportEvents.IMPORT_CANCELLED);
  }

  this.scanManager_.reset();
  this.checkState_();
};

/**
 * Checks the environment and updates UI as needed.
 * @param {importer.ScanResult=} opt_scan If supplied,
 * @private
 */
importer.ImportController.prototype.checkState_ = function(opt_scan) {
  // If there is no Google Drive mount, Drive may be disabled
  // or the machine may be running in guest mode.
  if (!this.environment_.isGoogleDriveMounted()) {
    this.updateUi_(importer.ActivityState.HIDDEN);
    return;
  }

  if (!!this.activeImport_) {
    this.updateUi_(importer.ActivityState.IMPORTING, this.activeImport_.scan);
    return;
  }

  // If we don't have an existing scan, we'll try to create
  // one. When we do end up creating one (not getting
  // one from the cache) it'll be empty...even if there is
  // a current selection. This is because scans are
  // resolved asynchronously. And we like it that way.
  // We'll get notification when the scan is updated. When
  // that happens, we'll be called back with opt_scan
  // set to that scan....and subsequently skip over this
  // block to update the UI.
  if (!opt_scan) {
    // NOTE, that tryScan_ lazily initializes scans...so if
    // no scan is returned, no scan is possible for the
    // current context.
    var scan = this.tryScan_();
    // If no scan is created, then no scan is possible in
    // the current context...so hide the UI.
    if (!scan) {
      this.updateUi_(importer.ActivityState.HIDDEN);
    }
    return;
  }

  // At this point we have an existing scan, and a relatively
  // validate environment for an import...so we'll proceed
  // with making updates to the UI.
  if (!opt_scan.isFinal()) {
    this.updateUi_(importer.ActivityState.SCANNING, opt_scan);
    return;
  }

  if (opt_scan.getFileEntries().length === 0) {
    this.updateUi_(importer.ActivityState.NO_MEDIA);
    return;
  }

  // We have a final scan that is either too big, or juuuussttt right.
  this.fitsInAvailableSpace_(opt_scan).then(
      /** @param {boolean} fits */
      function(fits) {
          if (!fits) {
            this.updateUi_(
                importer.ActivityState.INSUFFICIENT_SPACE,
                opt_scan);
            return;
          }

        this.updateUi_(
            importer.ActivityState.READY,  // to import...
            opt_scan);
      }.bind(this))
      .catch(importer.getLogger().catcher('import-controller-check-state'));
};

/**
 * @param {importer.ActivityState} activityState
 * @param {importer.ScanResult=} opt_scan
 * @private
 */
importer.ImportController.prototype.updateUi_ =
    function(activityState, opt_scan) {
  this.lastActivityState_ = activityState;
  this.commandWidget_.update(activityState, opt_scan);
};

/**
 * @return {boolean} true if the current directory is scan eligible.
 * @private
 */
importer.ImportController.prototype.isCurrentDirectoryScannable_ =
    function() {
  var directory = this.environment_.getCurrentDirectory();
  return !!directory &&
      importer.isMediaDirectory(directory, this.environment_);
};

/**
 * @param {!importer.ScanResult} scanResult
 * @return {!Promise<boolean>} True if the files in scan will fit
 *     in available local storage space.
 * @private
 */
importer.ImportController.prototype.fitsInAvailableSpace_ =
    function(scanResult) {
  return this.environment_.getFreeStorageSpace().then(
      /** @param {number} availableSpace In bytes. */
      function(availableSpace) {
        // TODO(smckay): We might want to disqualify some small amount
        // storage in this calculation on the assumption that we
        // don't want to completely max out storage...even though
        // synced files will eventually be evicted from the cache.
        return availableSpace > scanResult.getStatistics().sizeBytes;
      });
};

/**
 * Attempts to scan the current context.
 *
 * @return {importer.ScanResult} A scan object,
 *     or null if scan is not possible in current context.
 * @private
 */
importer.ImportController.prototype.tryScan_ = function() {
  var entries = this.environment_.getSelection();
  if (entries.length) {
    if (entries.every(
        importer.isEligibleEntry.bind(null, this.environment_))) {
      return this.scanManager_.getSelectionScan(entries);
    }
  } else if (this.isCurrentDirectoryScannable_()) {
    return this.scanManager_.getDirectoryScan();
  }

  return null;
};

/**
 * Class that adapts from the new non-command button to the old
 * command style interface.
 *
 * @interface
 */
importer.CommandWidget = function() {};

/**
 * Install a listener that get's called when the user wants to initiate
 * import.
 *
 * @param {function(!importer.ClickSource)} listener
 */
importer.CommandWidget.prototype.addClickListener;

/**
 * @param {importer.ActivityState} activityState
 * @param {importer.ScanResult=} opt_scan
 */
importer.CommandWidget.prototype.update;

/**
 * Directly sets the visibility of the details panel.
 *
 * @param {boolean} visible
 */
importer.CommandWidget.prototype.setDetailsVisible;

/**
 * Toggles the visibility state of the details panel.
 */
importer.CommandWidget.prototype.toggleDetails;

/**
 * Sets the details banner visibility.
 */
importer.CommandWidget.prototype.setDetailsBannerVisible;

/**
 * @enum {string}
 */
importer.ClickSource = {
  CANCEL: 'cancel',
  DESTINATION: 'destination',
  IMPORT: 'import',
  MAIN: 'main',
  SIDE: 'side'
};

/**
 * Runtime implementation of CommandWidget.
 *
 * @constructor
 * @implements {importer.CommandWidget}
 * @struct
 */
importer.RuntimeCommandWidget = function() {

  /** @private {HTMLElement} */
  this.detailsPanel_ = document.getElementById('cloud-import-details');
  this.detailsPanel_.addEventListener(
      'transitionend',
      this.onDetailsTransitionEnd_.bind(this),
      false);

  // Any clicks on document outside of the details panel
  // result in the panel being hidden.
  document.onclick = this.onDetailsFocusLost_.bind(this);

  // Stop further propagation of click events.
  // This allows us to listen for *any other* clicks
  // to hide the panel.
  this.detailsPanel_.onclick = function(event) {
    event.stopPropagation();
  };

  /** @private {!Element} */
  this.comboButton_ = getRequiredElement('cloud-import-combo-button');

  /** @private {!Element} */
  this.mainButton_ = queryRequiredElement(
      '#cloud-import-button', this.comboButton_);
  this.mainButton_.onclick = this.onButtonClicked_.bind(
      this, importer.ClickSource.MAIN);

  /** @private {!PaperRipple}*/
  this.mainButtonRipple_ =
      /** @type {!PaperRipple} */ (queryRequiredElement(
          '.ripples > paper-ripple', this.comboButton_));

  /** @private {Element} */
  this.sideButton_ = queryRequiredElement(
      '#cloud-import-details-button', this.comboButton_);
  this.sideButton_.onclick = this.onButtonClicked_.bind(
      this, importer.ClickSource.SIDE);

  /** @private {!FilesToggleRipple} */
  this.sideButtonRipple_ =
      /** @type {!FilesToggleRipple} */ (queryRequiredElement(
          '.ripples > files-toggle-ripple', this.comboButton_));

  /** @private {Element} */
  this.importButton_ =
      document.querySelector('#cloud-import-details paper-button.import');
  this.importButton_.onclick = this.onButtonClicked_.bind(
      this, importer.ClickSource.IMPORT);

  /** @private {Element} */
  this.cancelButton_ =
      document.querySelector('#cloud-import-details paper-button.cancel');
  this.cancelButton_.onclick = this.onButtonClicked_.bind(
      this, importer.ClickSource.CANCEL);

  /** @private {Element} */
  this.statusContent_ =
      document.querySelector('#cloud-import-details .status .content');
  this.statusContent_.onclick = this.onButtonClicked_.bind(
      this, importer.ClickSource.DESTINATION);

  /** @private {Element} */
  this.toolbarIcon_ =
      document.querySelector('#cloud-import-button iron-icon');
  this.statusIcon_ =
      document.querySelector('#cloud-import-details .status iron-icon');

  /** @private {Element} */
  this.detailsBanner_ = document.querySelector('#cloud-import-details .banner');

  /** @private {Element} */
  this.progressContainer_ =
      document.querySelector('#cloud-import-details .progress');
  /** @private {Element} */
  this.progressBar_ =
      document.querySelector('#cloud-import-details .progress .value');

  /** @private {function(!importer.ClickSource)} */
  this.clickListener_;

  document.addEventListener('keydown', this.onKeyDown_.bind(this));
};

/**
 * Handles document scoped key-down events.
 * @param {Event} event Key event.
 * @private
 */
importer.RuntimeCommandWidget.prototype.onKeyDown_ = function(event) {
  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+001B':
      this.setDetailsVisible(false);
  }
};

/**
 * Ensures that a transitionend event gets sent out after a transition.  Similar
 * to ensureTransitionEndEvent (see ui/webui/resources/js/util.js) but sends a
 * standard-compliant rather than a webkit event.
 *
 * @param {!HTMLElement} element
 * @param {number} timeout In milliseconds.
 */
importer.RuntimeCommandWidget.ensureTransitionEndEvent =
    function(element, timeout) {
    var fired = false;
  element.addEventListener('transitionend', function f(e) {
    element.removeEventListener('transitionend', f);
    fired = true;
  });
  // Use a timeout of 400 ms.
  window.setTimeout(function() {
    if (!fired)
      cr.dispatchSimpleEvent(element, 'transitionend', true);
  }, timeout);
};

/** @override */
importer.RuntimeCommandWidget.prototype.addClickListener =
    function(listener) {
  this.clickListener_ = listener;
};

/**
 * @param {!importer.ClickSource} source
 * @param {Event} event Click event.
 * @private
 */
importer.RuntimeCommandWidget.prototype.onButtonClicked_ =
    function(source, event) {
  console.assert(!!this.clickListener_, 'Listener not set.');

  // Clear focus from the toolbar button after it is clicked.
  if (source === importer.ClickSource.MAIN)
    this.mainButton_.blur();

  switch (source) {
    case importer.ClickSource.MAIN:
    case importer.ClickSource.IMPORT:
    case importer.ClickSource.CANCEL:
      this.clickListener_(source);
      break;
    case importer.ClickSource.DESTINATION:
      // NOTE: The element identified by class 'destination-link'
      // comes and goes as the message in the UI changes.
      // For this reason we add a click listener on the *container*
      // and when handling clicks, check to see if the source
      // was an element marked up to look like a link.
      if (event.target.classList.contains('destination-link')) {
        this.clickListener_(source);
      }
    case importer.ClickSource.SIDE:
      this.toggleDetails();
      break;
    default:
      assertNotReached('Unhandled click source: ' + source);
  }

  event.stopPropagation();
};

importer.RuntimeCommandWidget.prototype.performMainButtonRippleAnimation =
    function() {
  this.mainButtonRipple_.simulatedRipple();
};

/** @override */
importer.RuntimeCommandWidget.prototype.toggleDetails = function() {
  this.setDetailsVisible(this.detailsPanel_.className === 'hidden');
};

/** @override */
importer.RuntimeCommandWidget.prototype.setDetailsBannerVisible =
    function(visible) {
  this.detailsBanner_.hidden = !visible;
};

/**
 * @param {boolean} visible
 */
importer.RuntimeCommandWidget.prototype.setDetailsVisible = function(visible) {
  this.sideButtonRipple_.activated = visible;

  if (visible) {
    // Align the detail panel horizontally to the dropdown button.
    if (document.documentElement.getAttribute('dir') === 'rtl') {
      var anchorLeft = this.comboButton_.getBoundingClientRect().left;
      if (anchorLeft)
        this.detailsPanel_.style.left = anchorLeft + 'px';
    } else {
      var availableWidth = document.body.getBoundingClientRect().width;
      var anchorRight = this.comboButton_.getBoundingClientRect().right;
      if (anchorRight)
        this.detailsPanel_.style.right = (availableWidth - anchorRight) + 'px';
    }

    this.detailsPanel_.hidden = false;

    // The following line is a hacky way to force the container to lay out
    // contents so that the transition is triggered.
    // This line MUST appear before clearing the classname.
    this.detailsPanel_.scrollTop += 0;

    this.detailsPanel_.className = '';
  } else {
    this.detailsPanel_.className = 'hidden';
    // transition duration is 200ms. Let's wait for 400ms.
    importer.RuntimeCommandWidget.ensureTransitionEndEvent(
        /** @type {!HTMLElement} */ (this.detailsPanel_),
        400);
  }
};

/** @private */
importer.RuntimeCommandWidget.prototype.onDetailsTransitionEnd_ =
    function() {
  if (this.detailsPanel_.className === 'hidden') {
    // if we simply make the panel invisible (via opacity)
    // it'll still be sitting there grabing mouse events
    // and so-on. So we *hide* hide it.
    this.detailsPanel_.hidden = true;
  }
};

/** @private */
importer.RuntimeCommandWidget.prototype.onDetailsFocusLost_ =
    function() {
  this.setDetailsVisible(false);
};

/** @override */
importer.RuntimeCommandWidget.prototype.update =
    function(activityState, opt_scan) {
  switch(activityState) {
    case importer.ActivityState.HIDDEN:
      this.setDetailsVisible(false);

      this.comboButton_.hidden = true;

      this.toolbarIcon_.setAttribute('icon', 'cloud-off');
      this.statusIcon_.setAttribute('icon', 'cloud-off');

      break;

    case importer.ActivityState.IMPORTING:
      console.assert(!!opt_scan, 'Scan not defined, but is required.');
      this.setDetailsVisible(false);

      this.mainButton_.setAttribute('aria-label', strf(
          'CLOUD_IMPORT_TOOLTIP_IMPORTING',
          opt_scan.getFileEntries().length));
      this.statusContent_.innerHTML = strf(
          'CLOUD_IMPORT_STATUS_IMPORTING',
          opt_scan.getFileEntries().length);

      this.comboButton_.hidden = false;
      this.importButton_.hidden = true;
      this.cancelButton_.hidden = false;
      this.progressContainer_.hidden = true;

      this.toolbarIcon_.setAttribute('icon', 'autorenew');
      this.statusIcon_.setAttribute('icon', 'autorenew');

      break;

    case importer.ActivityState.INSUFFICIENT_SPACE:
      console.assert(!!opt_scan, 'Scan not defined, but is required.');

      this.mainButton_.setAttribute('aria-label', strf(
          'CLOUD_IMPORT_TOOLTIP_INSUFFICIENT_SPACE'));
      this.statusContent_.innerHTML = strf(
          'CLOUD_IMPORT_STATUS_INSUFFICIENT_SPACE',
          opt_scan.getFileEntries().length);

      this.comboButton_.hidden = false;
      this.importButton_.hidden = true;
      this.cancelButton_.hidden = true;
      this.progressContainer_.hidden = true;

      this.toolbarIcon_.setAttribute('icon', 'cloud-off');
      this.statusIcon_.setAttribute('icon', 'image:photo');
      break;

    case importer.ActivityState.NO_MEDIA:
      this.mainButton_.setAttribute('aria-label', str(
          'CLOUD_IMPORT_TOOLTIP_NO_MEDIA'));
      this.statusContent_.innerHTML = str(
          'CLOUD_IMPORT_STATUS_NO_MEDIA');

      this.comboButton_.hidden = false;
      this.importButton_.hidden = true;
      this.cancelButton_.hidden = true;
      this.progressContainer_.hidden = true;

      this.toolbarIcon_.setAttribute('icon', 'cloud-done');
      this.statusIcon_.setAttribute('icon', 'cloud-done');
      break;

    case importer.ActivityState.READY:
      console.assert(!!opt_scan, 'Scan not defined, but is required.');

      this.mainButton_.setAttribute('aria-label', strf(
          'CLOUD_IMPORT_TOOLTIP_READY',
          opt_scan.getFileEntries().length));
      this.statusContent_.innerHTML = strf(
          'CLOUD_IMPORT_STATUS_READY',
          opt_scan.getFileEntries().length);

      this.comboButton_.hidden = false;
      this.importButton_.hidden = false;
      this.cancelButton_.hidden = true;
      this.progressContainer_.hidden = true;

      this.toolbarIcon_.setAttribute('icon', 'cloud-upload');
      this.statusIcon_.setAttribute('icon', 'image:photo');
      break;

    case importer.ActivityState.SCANNING:
      console.assert(!!opt_scan, 'Scan not defined, but is required.');

      this.mainButton_.setAttribute('aria-label', str(
          'CLOUD_IMPORT_TOOLTIP_SCANNING'));
      this.statusContent_.innerHTML = strf(
          'CLOUD_IMPORT_STATUS_SCANNING',
          opt_scan.getFileEntries().length);

      this.comboButton_.hidden = false;
      this.importButton_.hidden = true;
      // TODO(smckay): implement cancellation for scanning.
      this.cancelButton_.hidden = true;
      this.progressContainer_.hidden = false;

      var stats = opt_scan.getStatistics();
      this.progressBar_.style.width = stats.progress + '%';

      this.toolbarIcon_.setAttribute('icon', 'autorenew');
      this.statusIcon_.setAttribute('icon', 'autorenew');
      break;

    default:
      assertNotReached('Unrecognized response id: ' + activityState);
  }
};

/**
 * A cache for ScanResults.
 *
 * @constructor
 * @struct
 *
 * @param {!importer.ControllerEnvironment} environment
 * @param {!importer.MediaScanner} scanner
 */
importer.ScanManager = function(environment, scanner) {

  /** @private {!importer.ControllerEnvironment} */
  this.environment_ = environment;

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /**
   * The active files scan, if any.
   *
   * @private {importer.ScanResult}
   */
  this.selectionScan_ = null;

  /**
   * The active directory scan, if any.
   *
   * @private {importer.ScanResult}
   */
  this.directoryScan_ = null;
};

/**
 * Cancels and forgets all scans.
 */
importer.ScanManager.prototype.reset = function() {
  this.clearSelectionScan();
  this.clearDirectoryScan();
};

/**
 * @return {boolean} True if we have an existing selection scan.
 */
importer.ScanManager.prototype.hasSelectionScan = function() {
  return !!this.selectionScan_;
};

/**
 * Cancels and forgets the current selection scan, if any.
 */
importer.ScanManager.prototype.clearSelectionScan = function() {
  if (this.selectionScan_) {
    this.selectionScan_.cancel();
  }
  this.selectionScan_ = null;
};

/**
 * Cancels and forgets the current directory scan, if any.
 */
importer.ScanManager.prototype.clearDirectoryScan = function() {
  if (this.directoryScan_) {
    this.directoryScan_.cancel();
  }
  this.directoryScan_ = null;
};

/**
 * @return {importer.ScanResult} Current active scan, or null
 * if none.
 */
importer.ScanManager.prototype.getActiveScan = function() {
  return this.selectionScan_ || this.directoryScan_;
};

/**
 * @param {importer.ScanResult} scan
 * @return {boolean} True if scan is the active scan...meaning the current
 *     selection scan or the scan for the current directory.
 */
importer.ScanManager.prototype.isActiveScan = function(scan) {
  return scan === this.selectionScan_ || scan === this.directoryScan_;
};

/**
 * Returns the existing selection scan or a new one for the supplied
 * selection.
 *
 * @param {!Array<!FileEntry>} entries
 *
 * @return {!importer.ScanResult}
 */
importer.ScanManager.prototype.getSelectionScan = function(entries) {
  console.assert(!this.selectionScan_,
      'Cannot create new selection scan with another in the cache.');
  this.selectionScan_ = this.scanner_.scanFiles(entries);
  return this.selectionScan_;
};

/**
 * Returns a scan for the directory.
 *
 * @return {importer.ScanResult}
 */
importer.ScanManager.prototype.getDirectoryScan = function() {
  if (!this.directoryScan_) {
    var directory = this.environment_.getCurrentDirectory();
    if (directory) {
      this.directoryScan_ = this.scanner_.scanDirectory(
          /** @type {!DirectoryEntry} */ (directory));
    }
  }
  return this.directoryScan_;
};

/**
 * Interface abstracting away the concrete file manager available
 * to commands. By hiding file manager we make it easy to test
 * ImportController.
 *
 * @interface
 * @extends {VolumeManagerCommon.VolumeInfoProvider}
 */
importer.ControllerEnvironment = function() {};

/**
 * Returns the current file selection, if any. May be empty.
 * @return {!Array<!Entry>}
 */
importer.ControllerEnvironment.prototype.getSelection;

/**
 * Returns the directory entry for the current directory.
 * @return {DirectoryEntry|FakeEntry}
 */
importer.ControllerEnvironment.prototype.getCurrentDirectory;

/**
 * @param {!DirectoryEntry} entry
 */
importer.ControllerEnvironment.prototype.setCurrentDirectory;

/**
 * Returns true if the Drive mount is present.
 * @return {boolean}
 */
importer.ControllerEnvironment.prototype.isGoogleDriveMounted;

/**
 * Returns the available space for the local volume in bytes.
 * @return {!Promise<number>}
 */
importer.ControllerEnvironment.prototype.getFreeStorageSpace;

/**
 * Installs a 'window closed' listener. Listener is called just
 * just before the window is closed. Any business must be
 * done synchronously.
 * @param {function()} listener
 */
importer.ControllerEnvironment.prototype.addWindowCloseListener;

/**
 * Installs an 'unmount' listener. Listener is called with
 * the corresponding volume id when a volume is unmounted.
 * @param {function(string)} listener
 */
importer.ControllerEnvironment.prototype.addVolumeUnmountListener;

/**
 * Installs an 'directory-changed' listener. Listener is called when
 * the directory changed.
 * @param {function(!Event)} listener
 */
importer.ControllerEnvironment.prototype.addDirectoryChangedListener;

/**
 * Installs an 'selection-changed' listener. Listener is called when
 * user selected files is changed.
 * @param {function()} listener
 */
importer.ControllerEnvironment.prototype.addSelectionChangedListener;

/**
 * Reveals the import root directory (the parent of all import destinations)
 * in a new Files.app window.
 * E.g. "Chrome OS Cloud backup". Creates it if it doesn't exist.
 *
 * @return {!Promise} Resolves when the folder has been shown.
 */
importer.ControllerEnvironment.prototype.showImportRoot;

/**
 * Returns the date-stamped import destination directory in a new
 * Files.app window. E.g. "2015-12-04".
 * Creates it if it doesn't exist.
 *
 * @param {!Date} date The import date
 *
 * @return {!Promise<!DirectoryEntry>} Resolves when the folder is available.
 */
importer.ControllerEnvironment.prototype.getImportDestination;

/**
 * Reveals the date-stamped import destination directory in a new
 * Files.app window. E.g. "2015-12-04".
 * Creates it if it doesn't exist.
 *
 * @param {!Date} date The import date
 *
 * @return {!Promise} Resolves when the folder has been shown.
 */
importer.ControllerEnvironment.prototype.showImportDestination;

/**
 * Class providing access to various pieces of information in the
 * FileManager environment, like the current directory, volumeinfo lookup
 * By hiding file manager we make it easy to test importer.ImportController.
 *
 * @constructor
 * @implements {importer.ControllerEnvironment}
 *
 * @param {!FileManager} fileManager
 * @param {!FileSelectionHandler} selectionHandler
 */
importer.RuntimeControllerEnvironment =
    function(fileManager, selectionHandler) {
  /** @private {!FileManager} */
  this.fileManager_ = fileManager;

  /** @private {!FileSelectionHandler} */
  this.selectionHandler_ = selectionHandler;
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getSelection =
    function() {
  return this.fileManager_.getSelection().entries;
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getCurrentDirectory =
    function() {
  return this.fileManager_.getCurrentDirectoryEntry();
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.setCurrentDirectory =
    function(entry) {
  this.fileManager_.directoryModel.activateDirectoryEntry(entry);
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getVolumeInfo =
    function(entry) {
  return this.fileManager_.volumeManager.getVolumeInfo(entry);
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.isGoogleDriveMounted =
    function() {
  var drive = this.fileManager_.volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  return !!drive;
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getFreeStorageSpace =
    function() {
  // Checking DOWNLOADS returns the amount of available local storage space.
  var localVolumeInfo =
      this.fileManager_.volumeManager.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DOWNLOADS);
  return new Promise(
      function(resolve, reject) {
        chrome.fileManagerPrivate.getSizeStats(
            localVolumeInfo.volumeId,
            function(stats) {
              if (chrome.runtime.lastError) {
                reject('Failed to ascertain available free space: ' +
                    chrome.runtime.lastError.message);
                return;
              }
              resolve(assert(stats).remainingSize);
            });
      });
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.addWindowCloseListener =
    function(listener) {
  window.addEventListener('pagehide', listener);
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.addVolumeUnmountListener =
    function(listener) {
  // TODO(smckay): remove listeners when the page is torn down.
  chrome.fileManagerPrivate.onMountCompleted.addListener(
      /**
       * @param {!MountCompletedEvent} event
       * @this {importer.RuntimeControllerEnvironment}
       */
      function(event) {
        if (event.eventType === 'unmount') {
          listener(event.volumeMetadata.volumeId);
        }
      });
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.addDirectoryChangedListener =
    function(listener) {
  // TODO(smckay): remove listeners when the page is torn down.
  this.fileManager_.directoryModel.addEventListener(
      'directory-changed',
      listener);
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.addSelectionChangedListener =
    function(listener) {
  this.selectionHandler_.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      listener);
};

/**
 * Reveals the directory to the user in the Files app, either creating
 * a new window, or focusing if already open in a window.
 *
 * @param {!DirectoryEntry} directory
 * @private
 */
importer.RuntimeControllerEnvironment.prototype.revealDirectory_ =
    function(directory) {
  this.fileManager_.backgroundPage.launchFileManager(
      {currentDirectoryURL: directory.toURL()},
      /* App ID */ undefined);
};

/**
 * Retrieves the user's drive root.
 * @return {!Promise<!DirectoryEntry>}
 * @private
 */
importer.RuntimeControllerEnvironment.prototype.getDriveRoot_ = function() {
  var drive = this.fileManager_.volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  return /** @type {!Promise<!DirectoryEntry>} */ (drive.resolveDisplayRoot());
};

/**
 * Fetches (creating if necessary) the import destination subdirectory.
 * @return {!Promise<!DirectoryEntry>}
 * @private
 */
importer.RuntimeControllerEnvironment.prototype.demandCloudFolder_ =
    function(root) {
  return importer.demandChildDirectory(
      root,
      str('CLOUD_IMPORT_DESTINATION_FOLDER'));
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.showImportRoot =
    function() {
  return this.getDriveRoot_()
      .then(this.demandCloudFolder_.bind(this))
      .then(this.revealDirectory_.bind(this))
      .catch(importer.getLogger().catcher('import-root-provision-and-reveal'));
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getImportDestination =
    function(date) {
  return this.getDriveRoot_()
      .then(this.demandCloudFolder_.bind(this))
      .then(
          /**
           * @param {!DirectoryEntry} root
           * @return {!Promise<!DirectoryEntry>}
           */
          function(root) {
            return importer.demandChildDirectory(
                root,
                importer.getDirectoryNameForDate(date));
          })
      .catch(importer.getLogger().catcher('import-destination-provision'));
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.showImportDestination =
    function(date) {
  return this.getImportDestination(date)
      .then(this.revealDirectory_.bind(this))
      .catch(importer.getLogger().catcher('import-destination-reveal'));
};
