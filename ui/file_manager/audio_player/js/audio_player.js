// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Overrided metadata worker's path.
 * @type {string}
 */
ContentMetadataProvider.WORKER_SCRIPT = '/js/metadata_worker.js';

/**
 * @param {Element} container Container element.
 * @constructor
 */
function AudioPlayer(container) {
  this.container_ = container;
  this.volumeManager_ = new VolumeManagerWrapper(
      VolumeManagerWrapper.NonNativeVolumeStatus.ENABLED);
  this.metadataModel_ = MetadataModel.create(this.volumeManager_);
  this.selectedEntry_ = null;
  this.invalidTracks_ = {};

  this.model_ = new AudioPlayerModel();
  Object.observe(this.model_, function(changes) {
    for (var i = 0; i < changes.length; i++) {
      var change = changes[i];
      if (change.name == 'expanded' && change.type == 'update') {
        this.onModelExpandedChanged(change.oldValue, change.object.expanded);
        break;
      }
    }
  }.bind(this));

  this.entries_ = [];
  this.currentTrackIndex_ = -1;
  this.playlistGeneration_ = 0;

  /**
   * Whether if the playlist is expanded or not. This value is changed by
   * this.syncExpanded().
   * True: expanded, false: collapsed, null: unset.
   *
   * @type {?boolean}
   * @private
   */
  this.isExpanded_ = null;  // Initial value is null. It'll be set in load().

  this.player_ =
    /** @type {AudioPlayerElement} */ (document.querySelector('audio-player'));
  // TODO(yoshiki): Move tracks into the model.
  this.player_.tracks = [];
  this.player_.model = this.model_;

  // Run asynchronously after an event of model change is delivered.
  setTimeout(function() {
    this.errorString_ = '';
    this.offlineString_ = '';
    chrome.fileManagerPrivate.getStrings(function(strings) {
      container.ownerDocument.title = strings['AUDIO_PLAYER_TITLE'];
      this.errorString_ = strings['AUDIO_ERROR'];
      this.offlineString_ = strings['AUDIO_OFFLINE'];
      AudioPlayer.TrackInfo.DEFAULT_ARTIST =
          strings['AUDIO_PLAYER_DEFAULT_ARTIST'];
    }.bind(this));

    this.volumeManager_.addEventListener('externally-unmounted',
        this.onExternallyUnmounted_.bind(this));

    window.addEventListener('resize', this.onResize_.bind(this));

    // Show the window after DOM is processed.
    var currentWindow = chrome.app.window.current();
    if (currentWindow)
      setTimeout(currentWindow.show.bind(currentWindow), 0);
  }.bind(this), 0);
}

/**
 * Initial load method (static).
 */
AudioPlayer.load = function() {
  document.ondragstart = function(e) { e.preventDefault(); };

  AudioPlayer.instance =
      new AudioPlayer(document.querySelector('.audio-player'));

  reload();
};

/**
 * Unloads the player.
 */
function unload() {
  if (AudioPlayer.instance)
    AudioPlayer.instance.onUnload();
}

/**
 * Reloads the player.
 */
function reload() {
  AudioPlayer.instance.load(window.appState);
}

/**
 * Loads a new playlist.
 * @param {Playlist} playlist Playlist object passed via mediaPlayerPrivate.
 */
AudioPlayer.prototype.load = function(playlist) {
  this.playlistGeneration_++;
  this.currentTrackIndex_ = -1;

  // Save the app state, in case of restart. Make a copy of the object, so the
  // playlist member is not changed after entries are resolved.
  window.appState = JSON.parse(JSON.stringify(playlist));  // cloning
  util.saveAppState();

  this.isExpanded_ = this.model_.expanded;

  // Resolving entries has to be done after the volume manager is initialized.
  this.volumeManager_.ensureInitialized(function() {
    util.URLsToEntries(playlist.items, function(entries) {
      this.entries_ = entries;

      var position = playlist.position || 0;
      var time = playlist.time || 0;

      if (this.entries_.length == 0)
        return;

      var newTracks = [];
      var currentTracks = this.player_.tracks;
      var unchanged = (currentTracks.length === this.entries_.length);

      for (var i = 0; i != this.entries_.length; i++) {
        var entry = this.entries_[i];
        var onClick = this.select_.bind(this, i);
        newTracks.push(new AudioPlayer.TrackInfo(entry, onClick));

        if (unchanged && entry.toURL() !== currentTracks[i].url)
          unchanged = false;
      }

      if (!unchanged)
        this.player_.tracks = newTracks;

      // Run asynchronously, to makes it sure that the handler of the track list
      // is called, before the handler of the track index.
      setTimeout(function() {
        this.select_(position, !!time);

        // Load the selected track metadata first, then load the rest.
        this.loadMetadata_(position);
        for (i = 0; i != this.entries_.length; i++) {
          if (i != position)
            this.loadMetadata_(i);
        }
      }.bind(this), 0);
    }.bind(this));
  }.bind(this));
};

/**
 * Loads metadata for a track.
 * @param {number} track Track number.
 * @private
 */
AudioPlayer.prototype.loadMetadata_ = function(track) {
  this.fetchMetadata_(
      this.entries_[track], this.displayMetadata_.bind(this, track));
};

/**
 * Displays track's metadata.
 * @param {number} track Track number.
 * @param {Object} metadata Metadata object.
 * @param {string=} opt_error Error message.
 * @private
 */
AudioPlayer.prototype.displayMetadata_ = function(track, metadata, opt_error) {
  this.player_.tracks[track].setMetadata(metadata, opt_error);
};

/**
 * Closes audio player when a volume containing the selected item is unmounted.
 * @param {Event} event The unmount event.
 * @private
 */
AudioPlayer.prototype.onExternallyUnmounted_ = function(event) {
  if (!this.selectedEntry_)
    return;

  if (this.volumeManager_.getVolumeInfo(this.selectedEntry_) ===
      event.volumeInfo)
    window.close();
};

/**
 * Called on window is being unloaded.
 */
AudioPlayer.prototype.onUnload = function() {
  if (this.player_)
    this.player_.onPageUnload();

  if (this.volumeManager_)
    this.volumeManager_.dispose();
};

/**
 * Selects a new track to play.
 * @param {number} newTrack New track number.
 * @param {number} time New playback position (in second).
 * @private
 */
AudioPlayer.prototype.select_ = function(newTrack, time) {
  if (this.currentTrackIndex_ == newTrack) return;

  this.currentTrackIndex_ = newTrack;
  this.player_.currentTrackIndex = this.currentTrackIndex_;
  this.player_.audioController.time = time;

  // Run asynchronously after an event of current track change is delivered.
  setTimeout(function() {
    if (!window.appReopen)
      this.player_.audioElement.play();

    window.appState.position = this.currentTrackIndex_;
    window.appState.time = 0;
    util.saveAppState();

    var entry = this.entries_[this.currentTrackIndex_];

    this.fetchMetadata_(entry, function(metadata) {
      if (this.currentTrackIndex_ != newTrack)
        return;

      this.selectedEntry_ = entry;
    }.bind(this));
  }.bind(this), 0);
};

/**
 * @param {FileEntry} entry Track file entry.
 * @param {function(Object)} callback Callback.
 * @private
 */
AudioPlayer.prototype.fetchMetadata_ = function(entry, callback) {
  this.metadataModel_.get(
      [entry], ['mediaTitle', 'mediaArtist', 'present']).then(
      function(generation, metadata) {
        // Do nothing if another load happened since the metadata request.
        if (this.playlistGeneration_ == generation)
          callback(metadata[0]);
      }.bind(this, this.playlistGeneration_));
};

/**
 * Media error handler.
 * @private
 */
AudioPlayer.prototype.onError_ = function() {
  var track = this.currentTrackIndex_;

  this.invalidTracks_[track] = true;

  this.fetchMetadata_(
      this.entries_[track],
      function(metadata) {
        var error = (!navigator.onLine && !metadata.present) ?
            this.offlineString_ : this.errorString_;
        this.displayMetadata_(track, metadata, error);
        this.scheduleAutoAdvance_();
      }.bind(this));
};

/**
 * Toggles the expanded mode when resizing.
 *
 * @param {Event} event Resize event.
 * @private
 */
AudioPlayer.prototype.onResize_ = function(event) {
  if (!this.isExpanded_ &&
      window.innerHeight >= AudioPlayer.EXPANDED_MODE_MIN_HEIGHT) {
    this.isExpanded_ = true;
    this.model_.expanded = true;
  } else if (this.isExpanded_ &&
             window.innerHeight < AudioPlayer.EXPANDED_MODE_MIN_HEIGHT) {
    this.isExpanded_ = false;
    this.model_.expanded = false;
  }
};

/* Keep the below constants in sync with the CSS. */

/**
 * Window header size in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.HEADER_HEIGHT = 33;  // 32px + border 1px

/**
 * Track height in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.TRACK_HEIGHT = 44;

/**
 * Controls bar height in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.CONTROLS_HEIGHT = 73;  // 72px + border 1px

/**
 * Default number of items in the expanded mode.
 * @type {number}
 * @const
 */
AudioPlayer.DEFAULT_EXPANDED_ITEMS = 5;

/**
 * Minimum size of the window in the expanded mode in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.EXPANDED_MODE_MIN_HEIGHT = AudioPlayer.CONTROLS_HEIGHT +
                                       AudioPlayer.TRACK_HEIGHT * 2;

/**
 * Invoked when the 'expanded' property in the model is changed.
 * @param {boolean} oldValue Old value.
 * @param {boolean} newValue New value.
 */
AudioPlayer.prototype.onModelExpandedChanged = function(oldValue, newValue) {
  if (this.isExpanded_ !== null &&
      this.isExpanded_ === newValue)
    return;

  if (this.isExpanded_ && !newValue)
    this.lastExpandedHeight_ = window.innerHeight;

  if (this.isExpanded_ !== newValue) {
    this.isExpanded_ = newValue;
    this.syncHeight_();

    // Saves new state.
    window.appState.expanded = newValue;
    util.saveAppState();
  }
};

/**
 * @private
 */
AudioPlayer.prototype.syncHeight_ = function() {
  var targetHeight;

  if (this.model_.expanded) {
    // Expanded.
    if (!this.lastExpandedHeight_ ||
        this.lastExpandedHeight_ < AudioPlayer.EXPANDED_MODE_MIN_HEIGHT) {
      var expandedListHeight =
          Math.min(this.entries_.length, AudioPlayer.DEFAULT_EXPANDED_ITEMS) *
              AudioPlayer.TRACK_HEIGHT;
      targetHeight = AudioPlayer.CONTROLS_HEIGHT + expandedListHeight;
      this.lastExpandedHeight_ = targetHeight;
    } else {
      targetHeight = this.lastExpandedHeight_;
    }
  } else {
    // Not expanded.
    targetHeight = AudioPlayer.CONTROLS_HEIGHT + AudioPlayer.TRACK_HEIGHT;
  }

  window.resizeTo(window.innerWidth, targetHeight + AudioPlayer.HEADER_HEIGHT);
};

/**
 * Create a TrackInfo object encapsulating the information about one track.
 *
 * @param {FileEntry} entry FileEntry to be retrieved the track info from.
 * @param {function(MouseEvent)} onClick Click handler.
 * @constructor
 */
AudioPlayer.TrackInfo = function(entry, onClick) {
  this.url = entry.toURL();
  this.title = this.getDefaultTitle();
  this.artist = this.getDefaultArtist();

  // TODO(yoshiki): implement artwork.
  this.artwork = null;
  this.active = false;
};

/**
 * @return {string} Default track title (file name extracted from the url).
 */
AudioPlayer.TrackInfo.prototype.getDefaultTitle = function() {
  var title = this.url.split('/').pop();
  var dotIndex = title.lastIndexOf('.');
  if (dotIndex >= 0) title = title.substr(0, dotIndex);
  title = decodeURIComponent(title);
  return title;
};

/**
 * TODO(kaznacheev): Localize.
 */
AudioPlayer.TrackInfo.DEFAULT_ARTIST = 'Unknown Artist';

/**
 * @return {string} 'Unknown artist' string.
 */
AudioPlayer.TrackInfo.prototype.getDefaultArtist = function() {
  return AudioPlayer.TrackInfo.DEFAULT_ARTIST;
};

/**
 * @param {Object} metadata The metadata object.
 * @param {string} error Error string.
 */
AudioPlayer.TrackInfo.prototype.setMetadata = function(
    metadata, error) {
  // TODO(yoshiki): Handle error in better way.
  // TODO(yoshiki): implement artwork (metadata.thumbnail)
  this.title = metadata.mediaTitle || this.getDefaultTitle();
  this.artist = error || metadata.mediaArtist || this.getDefaultArtist();
};

// Starts loading the audio player.
window.addEventListener('polymer-ready', function(e) {
  AudioPlayer.load();
});
