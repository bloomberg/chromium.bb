// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * SingleFileDetailsPanel constructor.
 *
 * Represents grid for the details panel for a single file in Files app.
 * @constructor
 * @extends {HTMLDivElement}
 */
function SingleFileDetailsPanel() {
  throw new Error('Use SingleFileDetailsPanel.decorate');
}

/**
 * Inherits from HTMLDivElement.
 */
SingleFileDetailsPanel.prototype = {
  __proto__: HTMLDivElement.prototype,
  onFileSelectionChanged: function(entry) {
    this.setFileName_(entry);
    this.setGenericThumbnail_(entry);
    this.loadMetadata_(entry);
  },

  /**
   * Display filename for the filename.
   * @param {!FileEntry} entry
   * @private
   */
  setFileName_: function(entry) {
    this.filenameIcon_.setAttribute('file-type-icon', FileType.getIcon(entry));
    this.filename_.textContent = entry.name;
  },

  /**
   * Display generic thumbnail for the entry.
   * @param {!FileEntry} entry
   * @private
   */
  setGenericThumbnail_: function(entry) {
    if (entry.isDirectory) {
      this.thumbnail_.setAttribute('generic-thumbnail', 'folder');
    } else {
      var mediaType = FileType.getMediaType(entry);
      this.thumbnail_.setAttribute('generic-thumbnail', mediaType);
    }
  },

  /**
   * Load metadata for the entry.
   * @param {!FileEntry} entry
   * @private
   */
  loadMetadata_: function(entry) {
    this.ticket_++;
    var ticket = this.ticket_;
    this.thumbnail_.innerHTML = '';
    this.preview_ = null;
    this.thumbnail_.classList.toggle('loaded', false);
    this.metadataModel_.get([entry], SingleFileDetailsPanel.LOADING_ITEMS)
        .then(this.onMetadataLoaded_.bind(this, ticket, entry));
  },

  /**
   * Called when a metadata is fetched.
   * @param {number} ticket Ticket number.
   * @param {!FileEntry} entry
   * @param {!Array<!MetadataItem>} items metadata items
   * @private
   */
  onMetadataLoaded_: function(ticket, entry, items) {
    if (this.ticket_ !== ticket) {
      return;
    }
    var item = items[0];
    this.setPreview_(ticket, entry, item);
    this.setDetails_(entry, item);
  },

  /**
   * Display preview for the file entry.
   * @param {number} ticket Ticket number.
   * @param {!FileEntry} entry
   * @param {!MetadataItem} item metadata
   * @private
   */
  setPreview_: function(ticket, entry, item) {
    var type = FileType.getType(entry);
    var thumbnailUrl = item.thumbnailUrl || item.croppedThumbnailUrl;
    if (type.type === 'image') {
      if (item.externalFileUrl) {
        // it's in Google Drive. Use ImageLoader.
        if (item.thumbnailUrl) {
          this.loadThumbnailFromDrive_(item.thumbnailUrl,
              function (result) {
            if (ticket !== this.ticket_) {
              return;
            }
            if (result.status !== 'success') {
              return;
            }
            var url = result.data;
            var img = document.createElement('img');
            this.thumbnail_.appendChild(img);
            img.src = url;
            this.thumbnail_.classList.toggle('loaded', true);
          }.bind(this));
        }
      } else {
        var img = document.createElement('img');
        this.thumbnail_.appendChild(img);
        img.src = thumbnailUrl || entry.toURL();
        this.thumbnail_.classList.toggle('loaded', true);
      }
    } else if (type.type === 'video') {
      var video = document.createElement('video');
      video.controls = true;
      this.thumbnail_.appendChild(video);
      this.thumbnail_.classList.toggle('loaded', true);
      video.src = entry.toURL();
      if (item.externalFileUrl) {
        // it's in google drive.
        if (item.thumbnailUrl) {
          this.loadThumbnailFromDrive_(item.thumbnailUrl,
              function (result) {
            if (ticket !== this.ticket_) {
              return;
            }
            if (result.status !== 'success') {
              return;
            }
            video.poster = result.data;
          }.bind(this));
        }
      } else if (thumbnailUrl) {
        video.poster = thumbnailUrl;
      }
      this.preview_ = video;
    } else if (type.type === 'audio') {
      if (item.externalFileUrl) {
        // it's in google drive.
        if (item.thumbnailUrl) {
          this.loadThumbnailFromDrive_(item.thumbnailUrl,
              function (result) {
            if (ticket !== this.ticket_) {
              return;
            }
            if (result.status !== 'success') {
              return;
            }
            var url = result.data;
            var img = document.createElement('img');
            this.thumbnail_.appendChild(img);
            img.src = url;
            this.thumbnail_.classList.toggle('loaded', true);
          }.bind(this));
        }
      } else {
        this.loadContentMetadata_(entry, function(entry, items) {
          if (ticket !== this.ticket_) {
            return;
          }
          var item = items[0];
          if (!item.contentThumbnailUrl) {
            return;
          }
          var img = document.createElement('img');
          this.thumbnail_.appendChild(img);
          img.src = item.contentThumbnailUrl;
          this.thumbnail_.classList.toggle('loaded', true);
        }.bind(this));
      }
      var audio = document.createElement('audio');
      audio.controls = true;
      this.thumbnail_.appendChild(audio);
      audio.src = entry.toURL();
      this.preview_ = audio;
    }
  },

  /**
   * Load content metadata
   * @param {!FileEntry} entry
   * @param {function(!FileEntry, !Array<!MetadataItem>)} callback
   * @private
   */
  loadContentMetadata_: function(entry, callback) {
    this.metadataModel_.get([entry], SingleFileDetailsPanel.CONTENT_ITEMS)
        .then(callback.bind(null, entry));
  },

  /**
   * Load thumbnails from Drive.
   * @param {string} url Thumbnail url
   * @param {function({status: string, data:string, width:number,
   *     height:number})} callback
   * @private
   */
  loadThumbnailFromDrive_: function (url, callback) {
    ImageLoaderClient.getInstance().load(url, callback);
  },

  /**
   * Display detailed information from metadata item.
   * @param {!FileEntry} entry
   * @param {!MetadataItem} item metadata
   * @private
   */
  setDetails_: function(entry, item) {
    var elem;
    var self = this;
    var update = function(query, cond, thunk) {
      var elem = queryRequiredElement(query, self.list_);
      if (cond) {
        elem.classList.toggle('available', true);
        queryRequiredElement('.content', elem).textContent = thunk();
      } else {
        elem.classList.toggle('available', false);
      }
    };
    update('.modification-time', item.modificationTime, function() {
      return self.formatter_.formatModDate(item.modificationTime);
    });
    update('.file-size', item.size, function() {
      return self.formatter_.formatSize(item.size, item.hosted);
    });
    update('.image-size', item.imageWidth && item.imageHeight, function() {
      return item.imageWidth.toString()+"x"+item.imageHeight;
    });
    update('.media-title', item.mediaTitle, function() {
      return item.mediaTitle;
    });
    update('.media-artist', item.mediaArtist, function() {
      return item.mediaArtist;
    });
    // TODO(ryoh): Should we display more and more items?
  },
  /**
   * Called when visibility of this panel is changed.
   * @param {boolean} visibility True if the details panel is visible.
   */
  onVisibilityChanged: function(visibility) {
    if (!visibility) {
      if (this.preview_) {
        this.preview_.pause();
      }
    }
  },

  /**
   * Cancel loading task.
   */
  cancelLoading: function() {
    this.ticket_++;
  }
};

/**
 * Metadata items to display in details panel.
 * @const
 */
SingleFileDetailsPanel.LOADING_ITEMS = [
  'croppedThumbnailUrl',
  'customIconUrl',
  'dirty',
  'externalFileUrl',
  'hosted',
  'imageHeight',
  'imageRotation',
  'imageWidth',
  'mediaArtist',
  'mediaMimeType',
  'mediaTitle',
  'modificationTime',
  'size',
  'thumbnailUrl'
];

/**
 * Metadata items to display content metadatas in details panel.
 * @const
 */
SingleFileDetailsPanel.CONTENT_ITEMS = [
  'contentThumbnailUrl'
];

/**
 * Decorates an HTML element to be a SingleFileDetailsPanel.
 * @param {!HTMLDivElement} self The grid to decorate.
 * @param {!MetadataModel} metadataModel File system metadata.
 */
SingleFileDetailsPanel.decorate = function(self, metadataModel) {
  self.__proto__ = SingleFileDetailsPanel.prototype;
  self.metadataModel_ = metadataModel;
  self.formatter_ = new FileMetadataFormatter();
  self.filename_ = assertInstanceof(queryRequiredElement('.filename', self),
      HTMLDivElement);
  self.filenameIcon_ = assertInstanceof(
      queryRequiredElement('.filename-icon', self), HTMLDivElement);
  self.thumbnail_ = assertInstanceof(
      queryRequiredElement('.thumbnail', self), HTMLDivElement);
  self.list_ = queryRequiredElement('.details-list', self);
  /**
   * Preview element. Video or Audio element.
   * @private {HTMLMediaElement}
   */
  self.preview_ = null;
  /**
   * A ticket to display metadata.
   * It strictly increases as the user selects files.
   * Only the task that has the latest ticket can update the view.
   * @private {number}
   */
  self.ticket_ = 0;
};

/**
 * Sets date and time format.
 * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
 */
SingleFileDetailsPanel.prototype.setDateTimeFormat = function(use12hourClock) {
  this.formatter_.setDateTimeFormat(use12hourClock);
};
