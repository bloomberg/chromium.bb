// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * MultiFileDetailsPanel constructor.
 *
 * Represents grid for the details panel for a single file in Files app.
 * @constructor
 * @extends {HTMLDivElement}
 */
function MultiFileDetailsPanel() {
  throw new Error('Use MultiFileDetailsPanel.decorate');
}

/**
 * Inherits from HTMLDivElement.
 */
MultiFileDetailsPanel.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * @param {!Array<!FileEntry>} entries
   */
  onFileSelectionChanged: function(entries) {
    this.ticket_++;
    this.lastTargetEntries_ = entries;
    this.aggregateRateLimitter_.run();
  },

  startAggregation: function() {
    var aggregator = new MultiFileDetailsPanel.Aggregator(this.ticket_,
          this.metadataModel_,
          this.onAggregated_.bind(this));
    aggregator.enqueue(this.lastTargetEntries_);
  },

  /**
   * @param {number} ticket
   * @param {number} totalCount Total file count.
   * @param {number} totalSize Sum of file size.
   * @return {boolean} Whether should we continue the aggregation or not.
   */
  onAggregated_: function(ticket, totalCount, totalSize) {
    if (ticket !== this.ticket_) {
      return false;
    }
    this.lastTotalSize_ = totalSize;
    this.lastTotalCount_ = totalCount;
    this.viewUpdateRateLimitter_.run();
    return true;
  },

  /**
   * @private
   */
  updateView_: function() {
    queryRequiredElement('.file-size > .content', this.list_).textContent =
        this.formatter_.formatSize(this.lastTotalSize_);
    queryRequiredElement('.file-count > .content', this.list_).textContent =
        this.lastTotalCount_;
  },

  /**
   * Cancel loading task.
   */
  cancelLoading: function() {
    this.ticket_++;
  }
};

/**
 * Aggregator class. That count files and calculate a sum of file size.
 * @param {number} ticket
 * @param {!MetadataModel} metadataModel
 * @param {function(number, number, number)} callback Callback to update views.
 * @constructor
 */
MultiFileDetailsPanel.Aggregator = function(ticket, metadataModel, callback) {
  this.queue_ = [];
  this.totalCount_ = 0;
  this.totalSize_ = 0;
  this.ticket_ = ticket;
  this.metadataModel_ = metadataModel;
  this.callback_ = callback;
};

/**
 * Aggregates data of given files and enqueue directories to queue.
 * @param {!Array<!FileEntry>} entries
 * @private
 */
MultiFileDetailsPanel.Aggregator.prototype.enqueue = function(entries) {
  var files = [];
  var dirs = [];
  var self = this;
  for (var i = 0; i < entries.length; i++) {
    var entry = entries[i];
    if (entry.isFile) {
      files.push(entry);
    } else {
      dirs.push(entry);
    }
  }
  Array.prototype.push.apply(this.queue_, dirs);
  this.metadataModel_.get(files, ['size'])
    .then(function(metadatas) {
      for (var i = 0; i < metadatas.length; i++) {
        var metadata = metadatas[i];
        self.totalCount_++;
        self.totalSize_ += metadata.size;
      }
      if (self.update_()) {
        self.dequeue_();
      }
    }, function(err) {
      console.error(err);
    }).then(function () {
      if (self.update_()) {
        self.dequeue_();
      }
    });
};

/**
 * Updates views with current aggregate results.
 * @return {boolean} Whether we should continue the aggregation or not.
 * @private
 */
MultiFileDetailsPanel.Aggregator.prototype.update_ = function() {
  return this.callback_(this.ticket_, this.totalCount_, this.totalSize_);
};

/**
 * Gets one directory from queue and fetch metadata
 * @private
 */
MultiFileDetailsPanel.Aggregator.prototype.dequeue_ = function() {
  if (this.queue_.length === 0) {
    return;
  }
  var self = this;
  var next = this.queue_.shift();
  var reader = next.createReader();
  reader.readEntries(function(results) {
    self.enqueue(results);
  });
};

/**
 * Decorates an HTML element to be a MultiFileDetailsPanel.
 * @param {!HTMLDivElement} self The grid to decorate.
 * @param {!MetadataModel} metadataModel File system metadata.
 */
MultiFileDetailsPanel.decorate = function(self, metadataModel) {
  self.__proto__ = MultiFileDetailsPanel.prototype;
  self.formatter_ = new FileMetadataFormatter();
  self.metadataModel_ = metadataModel;
  self.ticket_ = 0;
  self.lastTotalSize_ = 0;
  self.lastTotalCount_ = 0;
  self.list_ = queryRequiredElement('.details-list', self);
  self.aggregateRateLimitter_ =
      new AsyncUtil.RateLimiter(self.startAggregation.bind(self));
  self.viewUpdateRateLimitter_ =
      new AsyncUtil.RateLimiter(self.updateView_.bind(self));
};
