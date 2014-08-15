// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Scanner of the entries.
 * @constructor
 */
function ContentScanner() {
  this.cancelled_ = false;
}

/**
 * Starts to scan the entries. For example, starts to read the entries in a
 * directory, or starts to search with some query on a file system.
 * Derived classes must override this method.
 *
 * @param {function(Array.<Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the scan is completed
 *     successfully.
 * @param {function(FileError)} errorCallback Called an error occurs.
 */
ContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
};

/**
 * Request cancelling of the running scan. When the cancelling is done,
 * an error will be reported from errorCallback passed to scan().
 */
ContentScanner.prototype.cancel = function() {
  this.cancelled_ = true;
};

/**
 * Scanner of the entries in a directory.
 * @param {DirectoryEntry} entry The directory to be read.
 * @constructor
 * @extends {ContentScanner}
 */
function DirectoryContentScanner(entry) {
  ContentScanner.call(this);
  this.entry_ = entry;
}

/**
 * Extends ContentScanner.
 */
DirectoryContentScanner.prototype.__proto__ = ContentScanner.prototype;

/**
 * Starts to read the entries in the directory.
 * @override
 */
DirectoryContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  if (!this.entry_ || util.isFakeEntry(this.entry_)) {
    // If entry is not specified or a fake, we cannot read it.
    errorCallback(util.createDOMError(
        util.FileError.INVALID_MODIFICATION_ERR));
    return;
  }

  metrics.startInterval('DirectoryScan');
  var reader = this.entry_.createReader();
  var readEntries = function() {
    reader.readEntries(
        function(entries) {
          if (this.cancelled_) {
            errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
            return;
          }

          if (entries.length === 0) {
            // All entries are read.
            metrics.recordInterval('DirectoryScan');
            successCallback();
            return;
          }

          entriesCallback(entries);
          readEntries();
        }.bind(this),
        errorCallback);
  }.bind(this);
  readEntries();
};

/**
 * Scanner of the entries for the search results on Drive File System.
 * @param {string} query The query string.
 * @constructor
 * @extends {ContentScanner}
 */
function DriveSearchContentScanner(query) {
  ContentScanner.call(this);
  this.query_ = query;
}

/**
 * Extends ContentScanner.
 */
DriveSearchContentScanner.prototype.__proto__ = ContentScanner.prototype;

/**
 * Delay in milliseconds to be used for drive search scan, in order to reduce
 * the number of server requests while user is typing the query.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.SCAN_DELAY_ = 200;

/**
 * Maximum number of results which is shown on the search.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.MAX_RESULTS_ = 100;

/**
 * Starts to search on Drive File System.
 * @override
 */
DriveSearchContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  var numReadEntries = 0;
  var readEntries = function(nextFeed) {
    chrome.fileBrowserPrivate.searchDrive(
        {query: this.query_, nextFeed: nextFeed},
        function(entries, nextFeed) {
          if (this.cancelled_) {
            errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
            return;
          }

          // TODO(tbarzic): Improve error handling.
          if (!entries) {
            console.error('Drive search encountered an error.');
            errorCallback(util.createDOMError(
                util.FileError.INVALID_MODIFICATION_ERR));
            return;
          }

          var numRemainingEntries =
              DriveSearchContentScanner.MAX_RESULTS_ - numReadEntries;
          if (entries.length >= numRemainingEntries) {
            // The limit is hit, so quit the scan here.
            entries = entries.slice(0, numRemainingEntries);
            nextFeed = '';
          }

          numReadEntries += entries.length;
          if (entries.length > 0)
            entriesCallback(entries);

          if (nextFeed === '')
            successCallback();
          else
            readEntries(nextFeed);
        }.bind(this));
  }.bind(this);

  // Let's give another search a chance to cancel us before we begin.
  setTimeout(
      function() {
        // Check cancelled state before read the entries.
        if (this.cancelled_) {
          errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
          return;
        }
        readEntries('');
      }.bind(this),
      DriveSearchContentScanner.SCAN_DELAY_);
};

/**
 * Scanner of the entries of the file name search on the directory tree, whose
 * root is entry.
 * @param {DirectoryEntry} entry The root of the search target directory tree.
 * @param {string} query The query of the search.
 * @constructor
 * @extends {ContentScanner}
 */
function LocalSearchContentScanner(entry, query) {
  ContentScanner.call(this);
  this.entry_ = entry;
  this.query_ = query.toLowerCase();
}

/**
 * Extends ContentScanner.
 */
LocalSearchContentScanner.prototype.__proto__ = ContentScanner.prototype;

/**
 * Starts the file name search.
 * @override
 */
LocalSearchContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  var numRunningTasks = 0;
  var error = null;
  var maybeRunCallback = function() {
    if (numRunningTasks === 0) {
      if (this.cancelled_)
        errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
      else if (error)
        errorCallback(error);
      else
        successCallback();
    }
  }.bind(this);

  var processEntry = function(entry) {
    numRunningTasks++;
    var onError = function(fileError) {
      if (!error)
        error = fileError;
      numRunningTasks--;
      maybeRunCallback();
    };

    var onSuccess = function(entries) {
      if (this.cancelled_ || error || entries.length === 0) {
        numRunningTasks--;
        maybeRunCallback();
        return;
      }

      // Filters by the query, and if found, run entriesCallback.
      var foundEntries = entries.filter(function(entry) {
        return entry.name.toLowerCase().indexOf(this.query_) >= 0;
      }.bind(this));
      if (foundEntries.length > 0)
        entriesCallback(foundEntries);

      // Start to process sub directories.
      for (var i = 0; i < entries.length; i++) {
        if (entries[i].isDirectory)
          processEntry(entries[i]);
      }

      // Read remaining entries.
      reader.readEntries(onSuccess, onError);
    }.bind(this);

    var reader = entry.createReader();
    reader.readEntries(onSuccess, onError);
  }.bind(this);

  processEntry(this.entry_);
};

/**
 * Scanner of the entries for the metadata search on Drive File System.
 * @param {DriveMetadataSearchContentScanner.SearchType} searchType The option
 *     of the search.
 * @constructor
 * @extends {ContentScanner}
 */
function DriveMetadataSearchContentScanner(searchType) {
  ContentScanner.call(this);
  this.searchType_ = searchType;
}

/**
 * Extends ContentScanner.
 */
DriveMetadataSearchContentScanner.prototype.__proto__ =
    ContentScanner.prototype;

/**
 * The search types on the Drive File System.
 * @enum {string}
 */
DriveMetadataSearchContentScanner.SearchType = Object.freeze({
  SEARCH_ALL: 'ALL',
  SEARCH_SHARED_WITH_ME: 'SHARED_WITH_ME',
  SEARCH_RECENT_FILES: 'EXCLUDE_DIRECTORIES',
  SEARCH_OFFLINE: 'OFFLINE'
});

/**
 * Starts to metadata-search on Drive File System.
 * @override
 */
DriveMetadataSearchContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  chrome.fileBrowserPrivate.searchDriveMetadata(
      {query: '', types: this.searchType_, maxResults: 500},
      function(results) {
        if (this.cancelled_) {
          errorCallback(util.createDOMError(util.FileError.ABORT_ERR));
          return;
        }

        if (!results) {
          console.error('Drive search encountered an error.');
          errorCallback(util.createDOMError(
              util.FileError.INVALID_MODIFICATION_ERR));
          return;
        }

        var entries = results.map(function(result) { return result.entry; });
        if (entries.length > 0)
          entriesCallback(entries);
        successCallback();
      }.bind(this));
};

/**
 * This class manages filters and determines a file should be shown or not.
 * When filters are changed, a 'changed' event is fired.
 *
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @param {boolean} showHidden If files starting with '.' are shown.
 * @constructor
 * @extends {cr.EventTarget}
 */
function FileFilter(metadataCache, showHidden) {
  /**
   * @type {MetadataCache}
   * @private
   */
  this.metadataCache_ = metadataCache;

  /**
   * @type Object.<string, Function>
   * @private
   */
  this.filters_ = {};
  this.setFilterHidden(!showHidden);

  // Do not show entries marked as 'deleted'.
  this.addFilter('deleted', function(entry) {
    var internal = this.metadataCache_.getCached(entry, 'internal');
    return !(internal && internal.deleted);
  }.bind(this));
}

/*
 * FileFilter extends cr.EventTarget.
 */
FileFilter.prototype = {__proto__: cr.EventTarget.prototype};

/**
 * @param {string} name Filter identifier.
 * @param {function(Entry)} callback A filter — a function receiving an Entry,
 *     and returning bool.
 */
FileFilter.prototype.addFilter = function(name, callback) {
  this.filters_[name] = callback;
  cr.dispatchSimpleEvent(this, 'changed');
};

/**
 * @param {string} name Filter identifier.
 */
FileFilter.prototype.removeFilter = function(name) {
  delete this.filters_[name];
  cr.dispatchSimpleEvent(this, 'changed');
};

/**
 * @param {boolean} value If do not show hidden files.
 */
FileFilter.prototype.setFilterHidden = function(value) {
  if (value) {
    this.addFilter(
        'hidden',
        function(entry) { return entry.name.substr(0, 1) !== '.'; }
    );
  } else {
    this.removeFilter('hidden');
  }
};

/**
 * @return {boolean} If the files with names starting with "." are not shown.
 */
FileFilter.prototype.isFilterHiddenOn = function() {
  return 'hidden' in this.filters_;
};

/**
 * @param {Entry} entry File entry.
 * @return {boolean} True if the file should be shown, false otherwise.
 */
FileFilter.prototype.filter = function(entry) {
  for (var name in this.filters_) {
    if (!this.filters_[name](entry))
      return false;
  }
  return true;
};

/**
 * File list.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @constructor
 * @extends {cr.ui.ArrayDataModel}
 */
function FileListModel(metadataCache) {
  cr.ui.ArrayDataModel.call(this, []);

  /**
   * Metadata cache.
   * @type {MetadataCache}
   */
  this.metadataCache_ = metadataCache;

  // Initialize compare functions.
  this.setCompareFunction('name', util.compareName);
  this.setCompareFunction('modificationTime', this.compareMtime_.bind(this));
  this.setCompareFunction('size', this.compareSize_.bind(this));
  this.setCompareFunction('type', this.compareType_.bind(this));
}

FileListModel.prototype = {
  __proto__: cr.ui.ArrayDataModel.prototype
};

/**
 * Compare by mtime first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareMtime_ = function(a, b) {
  var aCachedFilesystem = this.metadataCache_.getCached(a, 'filesystem');
  var aTime = aCachedFilesystem ? aCachedFilesystem.modificationTime : 0;

  var bCachedFilesystem = this.metadataCache_.getCached(b, 'filesystem');
  var bTime = bCachedFilesystem ? bCachedFilesystem.modificationTime : 0;

  if (aTime > bTime)
    return 1;

  if (aTime < bTime)
    return -1;

  return util.compareName(a, b);
};

/**
 * Compare by size first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareSize_ = function(a, b) {
  var aCachedFilesystem = this.metadataCache_.getCached(a, 'filesystem');
  var aSize = aCachedFilesystem ? aCachedFilesystem.size : 0;

  var bCachedFilesystem = this.metadataCache_.getCached(b, 'filesystem');
  var bSize = bCachedFilesystem ? bCachedFilesystem.size : 0;

  return aSize !== bSize ? aSize - bSize : util.compareName(a, b);
};

/**
 * Compare by type first, then by subtype and then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareType_ = function(a, b) {
  // Directories precede files.
  if (a.isDirectory !== b.isDirectory)
    return Number(b.isDirectory) - Number(a.isDirectory);

  var aType = FileType.typeToString(FileType.getType(a));
  var bType = FileType.typeToString(FileType.getType(b));

  var result = util.collator.compare(aType, bType);
  return result !== 0 ? result : util.compareName(a, b);
};

/**
 * A context of DirectoryContents.
 * TODO(yoshiki): remove this. crbug.com/224869.
 *
 * @param {FileFilter} fileFilter The file-filter context.
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @constructor
 */
function FileListContext(fileFilter, metadataCache) {
  /**
   * @type {FileListModel}
   */
  this.fileList = new FileListModel(metadataCache);

  /**
   * @type {MetadataCache}
   */
  this.metadataCache = metadataCache;

  /**
   * @type {FileFilter}
   */
  this.fileFilter = fileFilter;
}

/**
 * This class is responsible for scanning directory (or search results),
 * and filling the fileList. Different descendants handle various types of
 * directory contents shown: basic directory, drive search results, local search
 * results.
 * TODO(hidehiko): Remove EventTarget from this.
 *
 * @param {FileListContext} context The file list context.
 * @param {boolean} isSearch True for search directory contents, otherwise
 *     false.
 * @param {DirectoryEntry} directoryEntry The entry of the current directory.
 * @param {function():ContentScanner} scannerFactory The factory to create
 *     ContentScanner instance.
 * @constructor
 * @extends {cr.EventTarget}
 */
function DirectoryContents(context,
                           isSearch,
                           directoryEntry,
                           scannerFactory) {
  this.context_ = context;
  this.fileList_ = context.fileList;

  this.isSearch_ = isSearch;
  this.directoryEntry_ = directoryEntry;

  this.scannerFactory_ = scannerFactory;
  this.scanner_ = null;
  this.processNewEntriesQueue_ = new AsyncUtil.Queue();
  this.scanCancelled_ = false;

  this.lastSpaceInMetadataCache_ = 0;
}

/**
 * DirectoryContents extends cr.EventTarget.
 */
DirectoryContents.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Create the copy of the object, but without scan started.
 * @return {DirectoryContents} Object copy.
 */
DirectoryContents.prototype.clone = function() {
  return new DirectoryContents(
      this.context_,
      this.isSearch_,
      this.directoryEntry_,
      this.scannerFactory_);
};

/**
 * Disposes the reserved metadata cache.
 */
DirectoryContents.prototype.dispose = function() {
  this.context_.metadataCache.resizeBy(-this.lastSpaceInMetadataCache_);
};

/**
 * Make a space for current directory size in the metadata cache.
 *
 * @param {number} size The cache size to be set.
 * @private
 */
DirectoryContents.prototype.makeSpaceInMetadataCache_ = function(size) {
  this.context_.metadataCache.resizeBy(size - this.lastSpaceInMetadataCache_);
  this.lastSpaceInMetadataCache_ = size;
};

/**
 * Use a given fileList instead of the fileList from the context.
 * @param {Array|cr.ui.ArrayDataModel} fileList The new file list.
 */
DirectoryContents.prototype.setFileList = function(fileList) {
  if (fileList instanceof cr.ui.ArrayDataModel)
    this.fileList_ = fileList;
  else
    this.fileList_ = new cr.ui.ArrayDataModel(fileList);
  this.makeSpaceInMetadataCache_(this.fileList_.length);
};

/**
 * Use the filelist from the context and replace its contents with the entries
 * from the current fileList.
 */
DirectoryContents.prototype.replaceContextFileList = function() {
  if (this.context_.fileList !== this.fileList_) {
    var spliceArgs = this.fileList_.slice();
    var fileList = this.context_.fileList;
    spliceArgs.unshift(0, fileList.length);
    fileList.splice.apply(fileList, spliceArgs);
    this.fileList_ = fileList;
    this.makeSpaceInMetadataCache_(this.fileList_.length);
  }
};

/**
 * @return {boolean} If the scan is active.
 */
DirectoryContents.prototype.isScanning = function() {
  return this.scanner_ || this.processNewEntriesQueue_.isRunning();
};

/**
 * @return {boolean} True if search results (drive or local).
 */
DirectoryContents.prototype.isSearch = function() {
  return this.isSearch_;
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for current directory. In case of
 *     search -- the top directory from which search is run.
 */
DirectoryContents.prototype.getDirectoryEntry = function() {
  return this.directoryEntry_;
};

/**
 * Start directory scan/search operation. Either 'scan-completed' or
 * 'scan-failed' event will be fired upon completion.
 *
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 */
DirectoryContents.prototype.scan = function(refresh) {
  /**
   * Invoked when the scanning is completed successfully.
   * @this {DirectoryContents}
   */
  function completionCallback() {
    this.onScanFinished_();
    this.onScanCompleted_();
  }

  /**
   * Invoked when the scanning is finished but is not completed due to error.
   * @this {DirectoryContents}
   */
  function errorCallback() {
    this.onScanFinished_();
    this.onScanError_();
  }

  // TODO(hidehiko,mtomasz): this scan method must be called at most once.
  // Remove such a limitation.
  this.scanner_ = this.scannerFactory_();
  this.scanner_.scan(this.onNewEntries_.bind(this, refresh),
                     completionCallback.bind(this),
                     errorCallback.bind(this));
};

/**
 * Cancels the running scan.
 */
DirectoryContents.prototype.cancelScan = function() {
  if (this.scanCancelled_)
    return;
  this.scanCancelled_ = true;
  if (this.scanner_)
    this.scanner_.cancel();

  this.onScanFinished_();

  this.processNewEntriesQueue_.cancel();
  cr.dispatchSimpleEvent(this, 'scan-cancelled');
};

/**
 * Called when the scanning by scanner_ is done, even when the scanning is
 * succeeded or failed. This is called before completion (or error) callback.
 *
 * @private
 */
DirectoryContents.prototype.onScanFinished_ = function() {
  this.scanner_ = null;

  this.processNewEntriesQueue_.run(function(callback) {
    // TODO(yoshiki): Here we should fire the update event of changed
    // items. Currently we have a method this.fileList_.updateIndex() to
    // fire an event, but this method takes only 1 argument and invokes sort
    // one by one. It is obviously time wasting. Instead, we call sort
    // directory.
    // In future, we should implement a good method like updateIndexes and
    // use it here.
    var status = this.fileList_.sortStatus;
    if (status)
      this.fileList_.sort(status.field, status.direction);

    callback();
  }.bind(this));
};

/**
 * Called when the scanning by scanner_ is succeeded.
 * @private
 */
DirectoryContents.prototype.onScanCompleted_ = function() {
  if (this.scanCancelled_)
    return;

  this.processNewEntriesQueue_.run(function(callback) {
    // Call callback first, so isScanning() returns false in the event handlers.
    callback();

    cr.dispatchSimpleEvent(this, 'scan-completed');
  }.bind(this));
};

/**
 * Called in case scan has failed. Should send the event.
 * @private
 */
DirectoryContents.prototype.onScanError_ = function() {
  if (this.scanCancelled_)
    return;

  this.processNewEntriesQueue_.run(function(callback) {
    // Call callback first, so isScanning() returns false in the event handlers.
    callback();
    cr.dispatchSimpleEvent(this, 'scan-failed');
  }.bind(this));
};

/**
 * Called when some chunk of entries are read by scanner.
 *
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 * @param {Array.<Entry>} entries The list of the scanned entries.
 * @private
 */
DirectoryContents.prototype.onNewEntries_ = function(refresh, entries) {
  if (this.scanCancelled_)
    return;

  var entriesFiltered = [].filter.call(
      entries, this.context_.fileFilter.filter.bind(this.context_.fileFilter));

  // Caching URL to reduce a number of calls of toURL in sort.
  // This is a temporary solution. We need to fix a root cause of slow toURL.
  // See crbug.com/370908 for detail.
  entriesFiltered.forEach(function(entry) { entry.cachedUrl = entry.toURL(); });

  if (entriesFiltered.length === 0)
    return;

  // Enlarge the cache size into the new filelist size.
  var newListSize = this.fileList_.length + entriesFiltered.length;
  this.makeSpaceInMetadataCache_(newListSize);

  this.processNewEntriesQueue_.run(function(callbackOuter) {
    var finish = function() {
      if (!this.scanCancelled_) {
        // Update the filelist without waiting the metadata.
        this.fileList_.push.apply(this.fileList_, entriesFiltered);
        cr.dispatchSimpleEvent(this, 'scan-updated');
      }
      callbackOuter();
    }.bind(this);
    // Because the prefetchMetadata can be slow, throttling by splitting entries
    // into smaller chunks to reduce UI latency.
    // TODO(hidehiko,mtomasz): This should be handled in MetadataCache.
    var MAX_CHUNK_SIZE = 25;
    var prefetchMetadataQueue = new AsyncUtil.ConcurrentQueue(4);
    for (var i = 0; i < entriesFiltered.length; i += MAX_CHUNK_SIZE) {
      if (prefetchMetadataQueue.isCancelled())
        break;

      var chunk = entriesFiltered.slice(i, i + MAX_CHUNK_SIZE);
      prefetchMetadataQueue.run(function(chunk, callbackInner) {
        this.prefetchMetadata(chunk, refresh, function() {
          if (!prefetchMetadataQueue.isCancelled()) {
            if (this.scanCancelled_)
              prefetchMetadataQueue.cancel();
          }

          // Checks if this is the last task.
          if (prefetchMetadataQueue.getWaitingTasksCount() === 0 &&
              prefetchMetadataQueue.getRunningTasksCount() === 1) {
            // |callbackOuter| in |finish| must be called before
            // |callbackInner|, to prevent double-calling.
            finish();
          }

          callbackInner();
        }.bind(this));
      }.bind(this, chunk));
    }
  }.bind(this));
};

/**
 * @param {Array.<Entry>} entries Files.
 * @param {boolean} refresh True to refresh metadata, or false to use cached
 *     one.
 * @param {function(Object)} callback Callback on done.
 */
DirectoryContents.prototype.prefetchMetadata =
    function(entries, refresh, callback) {
  var TYPES = 'filesystem|drive';
  if (refresh)
    this.context_.metadataCache.getLatest(entries, TYPES, callback);
  else
    this.context_.metadataCache.get(entries, TYPES, callback);
};

/**
 * Creates a DirectoryContents instance to show entries in a directory.
 *
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} directoryEntry The current directory entry.
 * @return {DirectoryContents} Created DirectoryContents instance.
 */
DirectoryContents.createForDirectory = function(context, directoryEntry) {
  return new DirectoryContents(
      context,
      false,  // Non search.
      directoryEntry,
      function() {
        return new DirectoryContentScanner(directoryEntry);
      });
};

/**
 * Creates a DirectoryContents instance to show the result of the search on
 * Drive File System.
 *
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} directoryEntry The current directory entry.
 * @param {string} query Search query.
 * @return {DirectoryContents} Created DirectoryContents instance.
 */
DirectoryContents.createForDriveSearch = function(
    context, directoryEntry, query) {
  return new DirectoryContents(
      context,
      true,  // Search.
      directoryEntry,
      function() {
        return new DriveSearchContentScanner(query);
      });
};

/**
 * Creates a DirectoryContents instance to show the result of the search on
 * Local File System.
 *
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} directoryEntry The current directory entry.
 * @param {string} query Search query.
 * @return {DirectoryContents} Created DirectoryContents instance.
 */
DirectoryContents.createForLocalSearch = function(
    context, directoryEntry, query) {
  return new DirectoryContents(
      context,
      true,  // Search.
      directoryEntry,
      function() {
        return new LocalSearchContentScanner(directoryEntry, query);
      });
};

/**
 * Creates a DirectoryContents instance to show the result of metadata search
 * on Drive File System.
 *
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} fakeDirectoryEntry Fake directory entry representing
 *     the set of result entries. This serves as a top directory for the
 *     search.
 * @param {DriveMetadataSearchContentScanner.SearchType} searchType The type of
 *     the search. The scanner will restricts the entries based on the given
 *     type.
 * @return {DirectoryContents} Created DirectoryContents instance.
 */
DirectoryContents.createForDriveMetadataSearch = function(
    context, fakeDirectoryEntry, searchType) {
  return new DirectoryContents(
      context,
      true,  // Search
      fakeDirectoryEntry,
      function() {
        return new DriveMetadataSearchContentScanner(searchType);
      });
};
