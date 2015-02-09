// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/** @enum {string} */
importer.ResponseId = {
  EXECUTABLE: 'executable',
  HIDDEN: 'hidden',
  ACTIVE_IMPORT: 'active_import',
  INSUFFICIENT_SPACE: 'insufficient_space',
  NO_MEDIA: 'no_media',
  SCANNING: 'scanning'
};

/**
 * @typedef {{
 *   id: !importer.ResponseId,
 *   label: string,
 *   visible: boolean,
 *   executable: boolean,
 *   coreIcon: string
 * }}
 */
importer.CommandUpdate;

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
 */
importer.ImportController =
    function(environment, scanner, importRunner, commandWidget) {

  /** @private {!importer.ControllerEnvironment} */
  this.environment_ = environment;

  /** @private {!importer.ImportRunner} */
  this.importRunner_ = importRunner;

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /** @private {!importer.CommandWidget} */
  this.commandWidget_ = commandWidget;

  /** @type {!importer.ScanManager} */
  this.scanManager_ = new importer.ScanManager(environment, scanner);

  /**
   * The active import task, if any.
   * @private {importer.MediaImportHandler.ImportTask}
   */
  this.activeImportTask_ = null;

  var listener = this.onScanEvent_.bind(this);
  this.scanner_.addObserver(listener);
  // Remove the observer when the foreground window is closed.
  window.addEventListener(
      'pagehide',
      function() {
        this.scanner_.removeObserver(listener);
      }.bind(this));

  this.environment_.addVolumeUnmountListener(
      this.onVolumeUnmounted_.bind(this));

  this.environment_.addDirectoryChangedListener(
      this.onDirectoryChanged_.bind(this));

  this.environment_.addSelectionChangedListener(
      this.onSelectionChanged_.bind(this));

  this.commandWidget_.addImportClickedListener(
      this.execute.bind(this));
};

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
      this.scanManager_.reset();
    case importer.ScanEvent.FINALIZED:
    case importer.ScanEvent.UPDATED:
      this.checkState_(scan);
      break;
  }
};

/**
 * @param {string} volumeId
 * @private
 */
importer.ImportController.prototype.onVolumeUnmounted_ = function(volumeId) {
  this.scanManager_.reset();
  this.checkState_();
};

/** @private */
importer.ImportController.prototype.onDirectoryChanged_ = function() {
  this.scanManager_.clearSelectionScan();
  if (this.isCurrentDirectoryScannable_()) {
    this.checkState_(
        this.scanManager_.getCurrentDirectoryScan());
  } else {
    this.checkState_();
  }
};

/** @private */
importer.ImportController.prototype.onSelectionChanged_ = function() {
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
        this.scanManager_.getCurrentDirectoryScan());
  } else {
    this.checkState_();
  }
};

/**
 * @param {!importer.MediaImportHandler.ImportTask} task
 * @private
 */
importer.ImportController.prototype.onImportFinished_ = function(task) {
  this.activeImportTask_ = null;
  this.scanManager_.reset();
  this.checkState_();
};

/**
 * Executes import against the current directory. Should only
 * be called when the current directory has been validated
 * by calling "update" on this class.
 */
importer.ImportController.prototype.execute = function() {
  console.assert(!this.activeImportTask_,
      'Cannot execute while an import task is already active.');
  metrics.recordEnum('CloudImport.UserAction', 'IMPORT_INITIATED');

  var scan = this.scanManager_.getActiveScan();
  assert(scan != null);

  var importTask = this.importRunner_.importFromScanResult(
      scan,
      importer.Destination.GOOGLE_DRIVE);

  this.activeImportTask_ = importTask;
  var taskFinished = this.onImportFinished_.bind(this, importTask);
  importTask.whenFinished.then(taskFinished).catch(taskFinished);
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
    this.updateUi_(importer.ResponseId.HIDDEN);
    return;
  }

  if (!!this.activeImportTask_) {
    this.updateUi_(importer.ResponseId.ACTIVE_IMPORT);
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
      this.updateUi_(importer.ResponseId.HIDDEN);
    }
    return;
  }

  // At this point we have an existing scan, and a relatively
  // validate environment for an import...so we'll proceed
  // with making updates to the UI.
  if (!opt_scan.isFinal()) {
    this.updateUi_(importer.ResponseId.SCANNING, opt_scan);
    return;
  }

  if (opt_scan.getFileEntries().length === 0) {
    this.updateUi_(importer.ResponseId.NO_MEDIA);
    return;
  }

  // We have a final scan that is either too big, or juuuussttt right.
  this.fitsInAvailableSpace_(opt_scan).then(
      /** @param {boolean} fits */
      function(fits) {
          if (!fits) {
            this.updateUi_(
                importer.ResponseId.INSUFFICIENT_SPACE,
                opt_scan);
            return;
          }

        this.updateUi_(
            importer.ResponseId.EXECUTABLE,
            opt_scan);
      }.bind(this));
};

/**
 * @param {importer.ResponseId} responseId
 * @param {importer.ScanResult=} opt_scan
 *
 * @return {!importer.CommandUpdate}
 * @private
 */
importer.ImportController.prototype.updateUi_ =
    function(responseId, opt_scan) {
  switch(responseId) {
    case importer.ResponseId.EXECUTABLE:
      this.commandWidget_.update({
        id: responseId,
        label: strf(
            'CLOUD_IMPORT_BUTTON_LABEL',
            opt_scan.getFileEntries().length),
        visible: true,
        executable: true,
        coreIcon: 'cloud-upload'
      });
      this.commandWidget_.updateDetails(opt_scan);
      break;
    case importer.ResponseId.HIDDEN:
      this.commandWidget_.update({
        id: responseId,
        visible: false,
        executable: false,
        label: '** SHOULD NOT BE VISIBLE **',
        coreIcon: 'cloud-off'
      });
      this.commandWidget_.setDetailsVisible(false);
      break;
    case importer.ResponseId.ACTIVE_IMPORT:
      this.commandWidget_.update({
        id: responseId,
        visible: true,
        executable: false,
        label: str('CLOUD_IMPORT_ACTIVE_IMPORT_BUTTON_LABEL'),
        coreIcon: 'swap-vert'
      });
      this.commandWidget_.setDetailsVisible(false);
      break;
    case importer.ResponseId.INSUFFICIENT_SPACE:
      this.commandWidget_.update({
        id: responseId,
        visible: true,
        executable: false,
        label: strf(
            'CLOUD_IMPORT_INSUFFICIENT_SPACE_BUTTON_LABEL',
            util.bytesToString(opt_scan.getTotalBytes())),
        coreIcon: 'report-problem'
      });
      this.commandWidget_.updateDetails(opt_scan);
      break;
    case importer.ResponseId.NO_MEDIA:
      this.commandWidget_.update({
        id: responseId,
        visible: true,
        executable: false,
        label: str('CLOUD_IMPORT_EMPTY_SCAN_BUTTON_LABEL'),
        coreIcon: 'cloud-done'
      });
      this.commandWidget_.updateDetails(
          /** @type {!importer.ScanResult} */ (opt_scan));
      break;
    case importer.ResponseId.SCANNING:
      this.commandWidget_.update({
        id: responseId,
        visible: true,
        executable: false,
        label: str('CLOUD_IMPORT_SCANNING_BUTTON_LABEL'),
        coreIcon: 'autorenew'
      });
      this.commandWidget_.updateDetails(
          /** @type {!importer.ScanResult} */ (opt_scan));
      break;
    default:
      assertNotReached('Unrecognized response id: ' + responseId);
  }
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
 * @return {!Promise.<boolean>} True if the files in scan will fit
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
        return availableSpace > scanResult.getTotalBytes();
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
    return this.scanManager_.getCurrentDirectoryScan();
  }

  return null;
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
 * @return {!Array.<!Entry>}
 */
importer.ControllerEnvironment.prototype.getSelection;

/**
 * Returns the directory entry for the current directory.
 * @return {!DirectoryEntry}
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
 * @return {!Promise.<number>}
 */
importer.ControllerEnvironment.prototype.getFreeStorageSpace;

/**
 * Installs an 'unmount' listener. Listener is called with
 * the corresponding volume id when a volume is unmounted.
 * @param {function(string)} listener
 */
importer.ControllerEnvironment.prototype.addVolumeUnmountListener;

/**
 * Installs an 'directory-changed' listener. Listener is called when
 * the directory changed.
 * @param {function()} listener
 */
importer.ControllerEnvironment.prototype.addDirectoryChangedListener;

/**
 * Installs an 'selection-changed' listener. Listener is called when
 * user selected files is changed.
 * @param {function()} listener
 */
importer.ControllerEnvironment.prototype.addSelectionChangedListener;

/**
 * Class providing access to various pieces of information in the
 * FileManager environment, like the current directory, volumeinfo lookup
 * By hiding file manager we make it easy to test importer.ImportController.
 *
 * @constructor
 * @implements {importer.ControllerEnvironment}
 *
 * @param {!FileManager} fileManager
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
  return /** @type {!DirectoryEntry} */ (
      this.fileManager_.getCurrentDirectoryEntry());
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

importer.RuntimeControllerEnvironment.prototype.getFreeStorageSpace =
    function() {
  // NOTE: Checking Drive (instead of Downloads) actually returns
  // the amount of available cloud storage space.
  var localVolumeInfo =
      this.fileManager_.volumeManager.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DOWNLOADS);
  return new Promise(
      function(resolve, reject) {
        chrome.fileManagerPrivate.getSizeStats(
            localVolumeInfo.volumeId,
            /** @param {Object} stats */
            function(stats) {
              if (stats && !chrome.runtime.lastError) {
                resolve(stats.remainingSize);
              } else {
                reject('Failed to ascertain available free space.');
              }
            });
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
 * @param {function()} listener
 */
importer.CommandWidget.prototype.addImportClickedListener;

/** @param {!importer.CommandUpdate} update */
importer.CommandWidget.prototype.update;

/** @param {!importer.ScanResult} scan */
importer.CommandWidget.prototype.updateDetails;

/** Resets details to default. */
importer.CommandWidget.prototype.resetDetails;

/**
 * Runtime implementation of CommandWidget.
 *
 * @constructor
 * @implements {importer.CommandWidget}
 * @struct
 */
importer.RuntimeCommandWidget = function() {

  /** @private {Element} */
  this.importButton_ = document.getElementById('cloud-import-button');
  this.importButton_.onclick = this.onImportClicked_.bind(this);

  /** @private {Element} */
  this.detailsButton_ = document.getElementById('cloud-import-details-button');
  this.detailsButton_.onclick = this.toggleDetails_.bind(this);

  /** @private {Element} */
  this.detailsImportButton_ =
      document.querySelector('#cloud-import-details .import');
  this.detailsImportButton_.onclick = this.onImportClicked_.bind(this);

  /** @private {Element} */
  this.detailsPanel_ = document.getElementById('cloud-import-details');

  /** @private {Element} */
  this.photoCount_ =
      document.querySelector('#cloud-import-details .photo-count');

  /** @private {Element} */
  this.spaceRequired_ =
      document.querySelector('#cloud-import-details .space-required');

  /** @private {Element} */
  this.icon_ = document.querySelector('#cloud-import-button core-icon');

  /** @private {function()} */
  this.importListener_;
};

/** @override */
importer.RuntimeCommandWidget.prototype.addImportClickedListener =
    function(listener) {
  console.assert(!this.importListener_);
  this.importListener_ = listener;
};

/** @private */
importer.RuntimeCommandWidget.prototype.onImportClicked_ = function() {
  console.assert(!!this.importListener_);
  this.importListener_();
};

/** @private */
importer.RuntimeCommandWidget.prototype.toggleDetails_ = function() {
  this.setDetailsVisible(this.detailsPanel_.className === 'offscreen');
};

importer.RuntimeCommandWidget.prototype.setDetailsVisible = function(visible) {
  if (visible) {
    this.detailsPanel_.className = '';
  } else {
    this.detailsPanel_.className = 'offscreen';
  }
};

/** @override */
importer.RuntimeCommandWidget.prototype.update = function(update) {
    this.importButton_.setAttribute('title', update.label);
    this.importButton_.disabled = !update.executable;
    this.importButton_.style.display =
        update.visible ? 'block' : 'none';

    this.icon_.setAttribute('icon', update.coreIcon);

    this.detailsButton_.disabled = !update.executable;
    this.detailsButton_.style.display =
        update.visible ? 'block' : 'none';
};

/** @override */
importer.RuntimeCommandWidget.prototype.updateDetails = function(scan) {
  this.photoCount_.textContent = scan.getFileEntries().length.toLocaleString();
  this.spaceRequired_.textContent = util.bytesToString(scan.getTotalBytes());
};

/** @override */
importer.RuntimeCommandWidget.prototype.resetDetails = function() {
  this.photoCount_.textContent = 0;
  this.spaceRequired_.textContent = 0;
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
   * A cache of selection scans by directory (url).
   *
   * @private {importer.ScanResult}
   */
  this.selectionScan_ = null;

  /**
   * A cache of scans by directory (url).
   *
   * @private {!Object.<string, !importer.ScanResult>}
   */
  this.directoryScans_ = {};
};

/**
 * Forgets all scans.
 */
importer.ScanManager.prototype.reset = function() {
  this.clearSelectionScan();
  this.clearDirectoryScans();
};

/**
 * Forgets the selection scans for the current directory.
 */
importer.ScanManager.prototype.clearSelectionScan = function() {
  this.selectionScan_ = null;
};

/**
 * Forgets directory scans.
 */
importer.ScanManager.prototype.clearDirectoryScans = function() {
  this.directoryScans_ = {};
};

/**
 * @return {importer.ScanResult} Current active scan, or null
 * if none.
 */
importer.ScanManager.prototype.getActiveScan = function() {
  return this.selectionScan_ ||
      this.directoryScans_[this.environment_.getCurrentDirectory().toURL()] ||
      null;
};

/**
 * @param {importer.ScanResult} scan
 * @return {boolean} True if scan is the active scan...meaning the current
 *     selection scan or the scan for the current directory.
 */
importer.ScanManager.prototype.isActiveScan = function(scan) {
  return scan === this.selectionScan_ ||
      scan === this.directoryScans_[
          this.environment_.getCurrentDirectory().toURL()];
};

/**
 * Returns the existing selection scan or a new one for the supplied
 * selection.
 *
 * @param {!Array.<!FileEntry>} entries
 *
 * @return {!importer.ScanResult}
 */
importer.ScanManager.prototype.getSelectionScan = function(entries) {
  console.assert(!this.selectionScan_,
      'Cannot create new selection scan with another in the cache.');
  this.selectionScan_ = this.scanner_.scan(entries);
  return this.selectionScan_;
};

/**
 * Returns a scan for the directory.
 *
 * @return {!importer.ScanResult}
 */
importer.ScanManager.prototype.getCurrentDirectoryScan = function() {
  var directory = this.environment_.getCurrentDirectory();
  var url = directory.toURL();
  var scan = this.directoryScans_[url];
  if (!scan) {
    scan = this.scanner_.scan([directory]);
    this.directoryScans_[url] = scan;
  }

  return scan;
};
