// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'audio-player',

  properties: {
    /**
     * Flag whether the audio is playing or paused. True if playing, or false
     * paused.
     */
    playing: {
      type: Boolean,
      observer: 'playingChanged',
      reflectToAttribute: true
    },

    /**
     * Current elapsed time in the current music in millisecond.
     */
    time: {
      type: Number,
      observer: 'timeChanged'
    },

    /**
     * Whether the shuffle button is ON.
     */
    shuffle: {
      type: Boolean,
      observer: 'shuffleChanged'
    },

    /**
     * Whether the repeat button is ON.
     */
    repeat: {
      type: Boolean,
      observer: 'repeatChanged'
    },

    /**
     * The audio volume. 0 is silent, and 100 is maximum loud.
     */
    volume: {
      type: Number,
      observer: 'volumeChanged'
    },

    /**
     * Whether the expanded button is ON.
     */
    expanded: {
      type: Boolean,
      observer: 'expandedChanged'
    },

    /**
     * Track index of the current track.
     */
    currentTrackIndex: {
      type: Number,
      observer: 'currentTrackIndexChanged'
    },

    /**
     * Model object of the Audio Player.
     * @type {AudioPlayerModel}
     */
    model: {
      type: Object,
      value: null,
      observer: 'modelChanged'
    },

    /**
     * URL of the current track. (exposed publicly for tests)
     */
    currenttrackurl: {
      type: String,
      value: '',
      reflectToAttribute: true
    },

    /**
     * The number of played tracks. (exposed publicly for tests)
     */
    playcount: {
      type: Number,
      value: 0,
      reflectToAttribute: true
    }
  },

  /**
   * Handles change event for shuffle mode.
   * @param {boolean} shuffle
   */
  shuffleChanged: function(shuffle) {
    if (this.model)
      this.model.shuffle = shuffle;
  },

  /**
   * Handles change event for repeat mode.
   * @param {boolean} repeat
   */
  repeatChanged: function(repeat) {
    if (this.model)
      this.model.repeat = repeat;
  },

  /**
   * Handles change event for audio volume.
   * @param {number} volume
   */
  volumeChanged: function(volume) {
    if (this.model)
      this.model.volume = volume;
  },

  /**
   * Handles change event for expanded state of track list.
   */
  expandedChanged: function(expanded) {
    if (this.model)
      this.model.expanded = expanded;
  },

  /**
   * Initializes an element. This method is called automatically when the
   * element is ready.
   */
  ready: function() {
    this.addEventListener('keydown', this.onKeyDown_.bind(this));

    this.$.audio.volume = 0;  // Temporary initial volume.
    this.$.audio.addEventListener('ended', this.onAudioEnded.bind(this));
    this.$.audio.addEventListener('error', this.onAudioError.bind(this));

    var onAudioStatusUpdatedBound = this.onAudioStatusUpdate_.bind(this);
    this.$.audio.addEventListener('timeupdate', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('ended', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('play', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('pause', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('suspend', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('abort', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('error', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('emptied', onAudioStatusUpdatedBound);
    this.$.audio.addEventListener('stalled', onAudioStatusUpdatedBound);
  },

  /**
   * Invoked when trackList.currentTrackIndex is changed.
   * @param {number} newValue new value.
   * @param {number} oldValue old value.
   */
  currentTrackIndexChanged: function(newValue, oldValue) {
    var currentTrackUrl = '';

    if (oldValue != newValue) {
      var currentTrack = this.$.trackList.getCurrentTrack();
      if (currentTrack && currentTrack.url != this.$.audio.src) {
        this.$.audio.src = currentTrack.url;
        currentTrackUrl = this.$.audio.src;
        if (this.playing)
          this.$.audio.play();
      }
    }

    // The attributes may be being watched, so we change it at the last.
    this.currenttrackurl = currentTrackUrl;
  },

  /**
   * Invoked when playing is changed.
   * @param {boolean} newValue new value.
   * @param {boolean} oldValue old value.
   */
  playingChanged: function(newValue, oldValue) {
    if (newValue) {
      if (!this.$.audio.src) {
        var currentTrack = this.$.trackList.getCurrentTrack();
        if (currentTrack && currentTrack.url != this.$.audio.src) {
          this.$.audio.src = currentTrack.url;
        }
      }

      if (this.$.audio.src) {
        this.currenttrackurl = this.$.audio.src;
        this.$.audio.play();
        return;
      }
    }

    // When the new status is "stopped".
    this.cancelAutoAdvance_();
    this.$.audio.pause();
    this.currenttrackurl = '';
    this.lastAudioUpdateTime_ = null;
  },

  /**
   * Invoked when the model changed.
   * @param {AudioPlayerModel} newModel New model.
   * @param {AudioPlayerModel} oldModel Old model.
   */
  modelChanged: function(newModel, oldModel) {
    // Setting up the UI
    if (newModel !== oldModel && newModel) {
      this.shuffle = newModel.shuffle;
      this.repeat = newModel.repeat;
      this.volume = newModel.volume;
      this.expanded = newModel.expanded;
    }
  },

  /**
   * Invoked when time is changed.
   * @param {number} newValue new time (in ms).
   * @param {number} oldValue old time (in ms).
   */
  timeChanged: function(newValue, oldValue) {
    // Ignores updates from the audio element.
    if (this.lastAudioUpdateTime_ === newValue)
      return;

    if (this.$.audio.readyState !== 0)
      this.$.audio.currentTime = this.time / 1000;
  },

  /**
   * Invoked when the next button in the controller is clicked.
   * This handler is registered in the 'on-click' attribute of the element.
   */
  onControllerNextClicked: function() {
    this.advance_(true /* forward */, true /* repeat */);
  },

  /**
   * Invoked when the previous button in the controller is clicked.
   * This handler is registered in the 'on-click' attribute of the element.
   */
  onControllerPreviousClicked: function() {
    this.advance_(false /* forward */, true /* repeat */);
  },

  /**
   * Invoked when the playback in the audio element is ended.
   * This handler is registered in this.ready().
   */
  onAudioEnded: function() {
    this.playcount++;
    this.advance_(true /* forward */, this.repeat);
  },

  /**
   * Invoked when the playback in the audio element gets error.
   * This handler is registered in this.ready().
   */
  onAudioError: function() {
    this.scheduleAutoAdvance_(true /* forward */, this.repeat);
  },

  /**
   * Invoked when the time of playback in the audio element is updated.
   * This handler is registered in this.ready().
   * @private
   */
  onAudioStatusUpdate_: function() {
    this.time = (this.lastAudioUpdateTime_ = this.$.audio.currentTime * 1000);
    this.duration = this.$.audio.duration * 1000;
    this.playing = !this.$.audio.paused;
  },

  /**
   * Invoked when receiving a request to replay the current music from the track
   * list element.
   */
  onReplayCurrentTrack: function() {
    // Changes the current time back to the beginning, regardless of the current
    // status (playing or paused).
    this.$.audio.currentTime = 0;
    this.time = 0;
  },

  /**
   * Goes to the previous or the next track.
   * @param {boolean} forward True if next, false if previous.
   * @param {boolean} repeat True if repeat-mode is enabled. False otherwise.
   * @private
   */
  advance_: function(forward, repeat) {
    this.cancelAutoAdvance_();

    var nextTrackIndex = this.$.trackList.getNextTrackIndex(forward, true);
    var isNextTrackAvailable =
        (this.$.trackList.getNextTrackIndex(forward, repeat) !== -1);

    this.playing = isNextTrackAvailable;

    // If there is only a single file in the list, 'currentTrackInde' is not
    // changed and the handler is not invoked. Instead, plays here.
    // TODO(yoshiki): clean up the code around here.
    if (isNextTrackAvailable &&
        this.$.trackList.currentTrackIndex == nextTrackIndex) {
      this.$.audio.play();
    }

    this.$.trackList.currentTrackIndex = nextTrackIndex;
  },

  /**
   * Timeout ID of auto advance. Used internally in scheduleAutoAdvance_() and
   *     cancelAutoAdvance_().
   * @type {number?}
   * @private
   */
  autoAdvanceTimer_: null,

  /**
   * Schedules automatic advance to the next track after a timeout.
   * @param {boolean} forward True if next, false if previous.
   * @param {boolean} repeat True if repeat-mode is enabled. False otherwise.
   * @private
   */
  scheduleAutoAdvance_: function(forward, repeat) {
    this.cancelAutoAdvance_();
    var currentTrackIndex = this.currentTrackIndex;

    var timerId = setTimeout(
        function() {
          // If the other timer is scheduled, do nothing.
          if (this.autoAdvanceTimer_ !== timerId)
            return;

          this.autoAdvanceTimer_ = null;

          // If the track has been changed since the advance was scheduled, do
          // nothing.
          if (this.currentTrackIndex !== currentTrackIndex)
            return;

          // We are advancing only if the next track is not known to be invalid.
          // This prevents an endless auto-advancing in the case when all tracks
          // are invalid (we will only visit each track once).
          this.advance_(forward, repeat);
        }.bind(this),
        3000);

    this.autoAdvanceTimer_ = timerId;
  },

  /**
   * Cancels the scheduled auto advance.
   * @private
   */
  cancelAutoAdvance_: function() {
    if (this.autoAdvanceTimer_) {
      clearTimeout(this.autoAdvanceTimer_);
      this.autoAdvanceTimer_ = null;
    }
  },

  /**
   * The list of the tracks in the playlist.
   *
   * When it changed, current operation including playback is stopped and
   * restarts playback with new tracks if necessary.
   *
   * @type {Array<TrackInfo>}
   */
  get tracks() {
    return this.$.trackList ? this.$.trackList.tracks : null;
  },
  set tracks(tracks) {
    if (this.$.trackList.tracks === tracks)
      return;

    this.cancelAutoAdvance_();

    this.$.trackList.tracks = tracks;
    var currentTrack = this.$.trackList.getCurrentTrack();
    if (currentTrack && currentTrack.url != this.$.audio.src) {
      this.$.audio.src = currentTrack.url;
      this.$.audio.play();
    }
  },

  /**
   * Invoked when the audio player is being unloaded.
   */
  onPageUnload: function() {
    this.$.audio.src = '';  // Hack to prevent crashing.
  },

  /**
   * Invoked when the 'keydown' event is fired.
   * @param {Event} event The event object.
   */
  onKeyDown_: function(event) {
    switch (event.keyIdentifier) {
      case 'Up':
        if (this.$.audioController.volumeSliderShown && this.model.volume < 100)
          this.model.volume += 1;
        break;
      case 'Down':
        if (this.$.audioController.volumeSliderShown && this.model.volume > 0)
          this.model.volume -= 1;
        break;
      case 'PageUp':
        if (this.$.audioController.volumeSliderShown && this.model.volume < 91)
          this.model.volume += 10;
        break;
      case 'PageDown':
        if (this.$.audioController.volumeSliderShown && this.model.volume > 9)
          this.model.volume -= 10;
        break;
      case 'MediaNextTrack':
        this.onControllerNextClicked();
        break;
      case 'MediaPlayPause':
        this.playing = !this.playing;
        break;
      case 'MediaPreviousTrack':
        this.onControllerPreviousClicked();
        break;
      case 'MediaStop':
        // TODO: Define "Stop" behavior.
        break;
    }
  },

  /**
   * Computes volume value for audio element. (should be in [0.0, 1.0])
   * @param {number} volume Volume which is set in the UI. ([0, 100])
   * @return {number}
   */
  computeAudioVolume_: function(volume) {
    return volume / 100;
  }
});
