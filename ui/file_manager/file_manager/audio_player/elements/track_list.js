// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  Polymer('track-list', {
    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      this.tracksObserver_ = new ArrayObserver(
          this.tracks,
          this.tracksValueChanged_.bind(this));

      window.addEventListener('resize', this.onWindowResize_.bind(this));
    },

    /**
     * Registers handlers for changing of external variables
     */
    observe: {
      'model.shuffle': 'onShuffleChanged',
    },

    /**
     * Model object of the Audio Player.
     * @type {AudioPlayerModel}
     */
    model: null,

    /**
     * List of tracks.
     * @type {Array.<AudioPlayer.TrackInfo>}
     */
    tracks: [],

    /**
     * Play order of the tracks. Each value is the index of 'this.tracks'.
     * @type {Array.<number>}
     */
    playOrder: [],

    /**
     * Track index of the current track.
     * If the tracks property is empty, it should be -1. Otherwise, be a valid
     * track number.
     *
     * @type {number}
     */
    currentTrackIndex: -1,

    /**
     * Invoked when 'shuffle' property is changed.
     * @param {boolean} oldValue Old value.
     * @param {boolean} newValue New value.
     */
    onShuffleChanged: function(oldValue, newValue) {
      this.generatePlayOrder(true /* keep the current track */);
    },

    /**
     * Invoked when the current track index is changed.
     * @param {number} oldValue old value.
     * @param {number} newValue new value.
     */
    currentTrackIndexChanged: function(oldValue, newValue) {
      if (oldValue === newValue)
        return;

      if (!isNaN(oldValue) && 0 <= oldValue && oldValue < this.tracks.length)
        this.tracks[oldValue].active = false;

      if (0 <= newValue && newValue < this.tracks.length) {
        var currentPlayOrder = this.playOrder.indexOf(newValue);
        if (currentPlayOrder !== -1) {
          // Success
          this.tracks[newValue].active = true;

          this.ensureTrackInViewport_(newValue /* trackIndex */);
          return;
        }
      }

      // Invalid index
      if (this.tracks.length === 0)
        this.currentTrackIndex = -1;
      else
        this.generatePlayOrder(false /* no need to keep the current track */);
    },

    /**
     * Invoked when 'tracks' property is changed.
     * @param {Array.<TrackInfo>} oldValue Old value.
     * @param {Array.<TrackInfo>} newValue New value.
     */
    tracksChanged: function(oldValue, newValue) {
      // Note: Sometimes both oldValue and newValue are null though the actual
      // values are not null. Maybe it's a bug of Polymer.

      // Re-register the observer of 'this.tracks'.
      this.tracksObserver_.close();
      this.tracksObserver_ = new ArrayObserver(this.tracks);
      this.tracksObserver_.open(this.tracksValueChanged_.bind(this));

      if (this.tracks.length !== 0) {
        // Restore the active track.
        if (this.currentTrackIndex !== -1 &&
            this.currentTrackIndex < this.tracks.length) {
          this.tracks[this.currentTrackIndex].active = true;
        }

        // Reset play order and current index.
        this.generatePlayOrder(false /* no need to keep the current track */);
      } else {
        this.playOrder = [];
        this.currentTrackIndex = -1;
      }
    },

    /**
     * Invoked when the value in the 'tracks' is changed.
     * @param {Array.<Object>} splices The detail of the change.
     */
    tracksValueChanged_: function(splices) {
      if (this.tracks.length === 0)
        this.currentTrackIndex = -1;
      else
        this.tracks[this.currentTrackIndex].active = true;
    },

    /**
     * Invoked when the track element is clicked.
     * @param {Event} event Click event.
     */
    trackClicked: function(event) {
      var index = ~~event.currentTarget.getAttribute('index');
      var track = this.tracks[index];
      if (track)
        this.selectTrack(track);
    },

    /**
     * Invoked when the window is resized.
     * @private
     */
    onWindowResize_: function() {
      this.ensureTrackInViewport_(this.currentTrackIndex);
    },

    /**
     * Scrolls the track list to ensure the given track in the viewport.
     * @param {number} trackIndex The index of the track to be in the viewport.
     * @private
     */
    ensureTrackInViewport_: function(trackIndex) {
      var trackSelector = '::shadow .track[index="' + trackIndex + '"]';
      var trackElement = this.querySelector(trackSelector);
      if (trackElement) {
        var viewTop = this.scrollTop;
        var viewHeight = this.clientHeight;
        var elementTop = trackElement.offsetTop;
        var elementHeight = trackElement.offsetHeight;

        if (elementTop < viewTop) {
          // Adjust the tops.
          this.scrollTop = elementTop;
        } else if (elementTop + elementHeight <= viewTop + viewHeight) {
          // The entire element is in the viewport. Do nothing.
        } else {
          // Adjust the bottoms.
          this.scrollTop = Math.max(0,
                                    (elementTop + elementHeight - viewHeight));
        }
      }
    },

    /**
     * Invoked when the track element is clicked.
     * @param {boolean} keepCurrentTrack Keep the current track or not.
     */
    generatePlayOrder: function(keepCurrentTrack) {
      console.assert((keepCurrentTrack !== undefined),
                     'The argument "forward" is undefined');

      if (this.tracks.length === 0) {
        this.playOrder = [];
        return;
      }

      // Creates sequenced array.
      this.playOrder =
          this.tracks.
          map(function(unused, index) { return index; });

      if (this.model && this.model.shuffle) {
        // Randomizes the play order array (Schwarzian-transform algorithm).
        this.playOrder =
            this.playOrder.
            map(function(a) {
              return {weight: Math.random(), index: a};
            }).
            sort(function(a, b) { return a.weight - b.weight }).
            map(function(a) { return a.index });

        if (keepCurrentTrack) {
          // Puts the current track at the beginning of the play order.
          this.playOrder =
              this.playOrder.filter(function(value) {
                return this.currentTrackIndex !== value;
              }, this);
          this.playOrder.splice(0, 0, this.currentTrackIndex);
        }
      }

      if (!keepCurrentTrack)
        this.currentTrackIndex = this.playOrder[0];
    },

    /**
     * Sets the current track.
     * @param {AudioPlayer.TrackInfo} track TrackInfo to be set as the current
     *     track.
     */
    selectTrack: function(track) {
      var index = -1;
      for (var i = 0; i < this.tracks.length; i++) {
        if (this.tracks[i].url === track.url) {
          index = i;
          break;
        }
      }
      if (index >= 0) {
        // TODO(yoshiki): Clean up the flow and the code around here.
        if (this.currentTrackIndex == index)
          this.replayCurrentTrack();
        else
          this.currentTrackIndex = index;
      }
    },

    /**
     * Request to replay the current music.
     */
    replayCurrentTrack: function() {
      this.fire('replay');
    },

    /**
     * Returns the current track.
     * @param {AudioPlayer.TrackInfo} track TrackInfo of the current track.
     */
    getCurrentTrack: function() {
      if (this.tracks.length === 0)
        return null;

      return this.tracks[this.currentTrackIndex];
    },

    /**
     * Returns the next (or previous) track in the track list. If there is no
     * next track, returns -1.
     *
     * @param {boolean} forward Specify direction: forward or previous mode.
     *     True: forward mode, false: previous mode.
     * @param {boolean} cyclic Specify if cyclically or not: It true, the first
     *     track is succeeding to the last track, otherwise no track after the
     *     last.
     * @return {number} The next track index.
     */
    getNextTrackIndex: function(forward, cyclic)  {
      if (this.tracks.length === 0)
        return -1;

      var defaultTrackIndex =
          forward ? this.playOrder[0] : this.playOrder[this.tracks.length - 1];

      var currentPlayOrder = this.playOrder.indexOf(this.currentTrackIndex);
      console.assert(
          (0 <= currentPlayOrder && currentPlayOrder < this.tracks.length),
          'Insufficient TrackList.playOrder. The current track is not on the ' +
            'track list.');

      var newPlayOrder = currentPlayOrder + (forward ? +1 : -1);
      if (newPlayOrder === -1 || newPlayOrder === this.tracks.length)
        return cyclic ? defaultTrackIndex : -1;

      var newTrackIndex = this.playOrder[newPlayOrder];
      console.assert(
          (0 <= newTrackIndex && newTrackIndex < this.tracks.length),
          'Insufficient TrackList.playOrder. New Play Order: ' + newPlayOrder);

      return newTrackIndex;
    },
  });  // Polymer('track-list') block
})();  // Anonymous closure
