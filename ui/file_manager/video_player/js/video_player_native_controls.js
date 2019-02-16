// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class NativeControlsVideoPlayer {
  constructor() {
    this.videos_ = null;
    this.currentPos_ = 0;
    this.videoElement_ = null;
  }

  /**
   * Initializes the video player window. This method must be called after DOM
   * initialization.
   * @param {!Array<!FileEntry>} videos List of videos.
   */
  prepare(videos) {
    this.videos_ = videos;

    // TODO: Move these setting to html and css file when
    // we are confident to remove the feature flag
    this.videoElement_ = document.createElement('video');
    this.videoElement_.controls = true;
    this.videoElement_.controlsList = 'nodownload';
    this.videoElement_.style.pointerEvents = 'auto';
    getRequiredElement('video-container').appendChild(this.videoElement_);

    // TODO: remove the element in html when remove the feature flag
    getRequiredElement('controls-wrapper').style.display = 'none';
    getRequiredElement('spinner-container').style.display = 'none';
    getRequiredElement('error-wrapper').style.display = 'none';
    getRequiredElement('thumbnail').style.display = 'none';
    getRequiredElement('cast-container').style.display = 'none';

    this.videoElement_.addEventListener('pause', this.onPause_.bind(this));

    this.preparePlayList_();
    this.addKeyControls_();
  }

  /**
   * Attach arrow box for previous/next track to document and set
   * 'multiple' attribute if user opens more than 1 videos.
   *
   * @private
   */
  preparePlayList_() {
    let videoPlayerElement = getRequiredElement('video-player');
    if (this.videos_.length > 1) {
      videoPlayerElement.setAttribute('multiple', true);
    } else {
      videoPlayerElement.removeAttribute('multiple');
    }

    let arrowRight = queryRequiredElement('.arrow-box .arrow.right');
    arrowRight.addEventListener(
        'click', this.advance_.bind(this, true /* next track */));
    let arrowLeft = queryRequiredElement('.arrow-box .arrow.left');
    arrowLeft.addEventListener(
        'click', this.advance_.bind(this, false /* previous track */));
  }

  /**
   * Add keyboard controls to document.
   *
   * @private
   */
  addKeyControls_() {
    document.addEventListener('keydown', (/** KeyboardEvent */ event) => {
      const key =
          (event.ctrlKey && event.shiftKey ? 'Ctrl-Shift-' : '') + event.key;
      switch (key) {
          // Handle debug shortcut keys.
        case 'Ctrl-Shift-I':  // Ctrl+Shift+I
          chrome.fileManagerPrivate.openInspector('normal');
          break;
        case 'Ctrl-Shift-J':  // Ctrl+Shift+J
          chrome.fileManagerPrivate.openInspector('console');
          break;
        case 'Ctrl-Shift-C':  // Ctrl+Shift+C
          chrome.fileManagerPrivate.openInspector('element');
          break;
        case 'Ctrl-Shift-B':  // Ctrl+Shift+B
          chrome.fileManagerPrivate.openInspector('background');
          break;

        case 'k':
        case 'MediaPlayPause':
          this.togglePlayState_();
          break;
        case 'MediaTrackNext':
          this.advance_(true /* next track */);
          break;
        case 'MediaTrackPrevious':
          this.advance_(false /* previous track */);
          break;
        case 'l':
          this.skip_(true /* forward */);
          break;
        case 'j':
          this.skip_(false /* backward */);
          break;
        case 'BrowserBack':
          chrome.app.window.current().close();
          break;
        case 'MediaStop':
          // TODO: Define "Stop" behavior.
          break;
      }
    });

    getRequiredElement('video-container')
        .addEventListener('click', (/** MouseEvent */ event) => {
          // Turn on loop mode when ctrl+click while the video is playing.
          // If the video is paused, ignore ctrl and play the video.
          if (event.ctrlKey && !this.videoElement_.paused) {
            this.setLoopedModeWithFeedback_(true);
            event.preventDefault();
          }
        });
  }

  /**
   * Skips forward/backward.
   * @param {boolean} forward Whether to skip forward or backward.
   * @private
   */
  skip_(forward) {
    let secondsToSkip = Math.min(
        NativeControlsVideoPlayer.PROGRESS_MAX_SECONDS_TO_SKIP,
        this.videoElement_.duration *
            NativeControlsVideoPlayer.PROGRESS_MAX_RATIO_TO_SKIP);

    if (!forward) {
      secondsToSkip *= -1;
    }

    this.videoElement_.currentTime = Math.max(
        Math.min(
            this.videoElement_.currentTime + secondsToSkip,
            this.videoElement_.duration),
        0);
  }

  /**
   * Toggle play/pause.
   *
   * @private
   */
  togglePlayState_() {
    if (this.videoElement_.paused) {
      this.videoElement_.play();
    } else {
      this.videoElement_.pause();
    }
  }

  /**
   * Set the looped mode with feedback.
   *
   * @param {boolean} on Whether enabled or not.
   * @private
   */
  setLoopedModeWithFeedback_(on) {
    this.videoElement_.loop = on;
    if (on) {
      this.showNotification_('VIDEO_PLAYER_LOOPED_MODE');
    }
  }

  /**
   * Briefly show a text notification at top left corner.
   *
   * @param {string} identifier String identifier.
   * @private
   */
  showNotification_(identifier) {
    getRequiredElement('toast-content').textContent =
        loadTimeData.getString(identifier);
    getRequiredElement('toast').show();
  }

  /**
   * Plays the first video.
   */
  playFirstVideo() {
    this.currentPos_ = 0;
    this.reloadCurrentVideo_(this.onFirstVideoReady_.bind(this));
  }

  /**
   * Called when the first video is ready after starting to load.
   * Make the video player app window the same dimension as the video source
   * Restrict the app window inside the screen.
   *
   * @private
   */
  onFirstVideoReady_() {
    let videoWidth = this.videoElement_.videoWidth;
    let videoHeight = this.videoElement_.videoHeight;

    let aspect = videoWidth / videoHeight;
    let newWidth = videoWidth;
    let newHeight = videoHeight;

    let shrinkX = newWidth / window.screen.availWidth;
    let shrinkY = newHeight / window.screen.availHeight;
    if (shrinkX > 1 || shrinkY > 1) {
      if (shrinkY > shrinkX) {
        newHeight = newHeight / shrinkY;
        newWidth = newHeight * aspect;
      } else {
        newWidth = newWidth / shrinkX;
        newHeight = newWidth / aspect;
      }
    }

    let oldLeft = window.screenX;
    let oldTop = window.screenY;
    let oldWidth = window.innerWidth;
    let oldHeight = window.innerHeight;

    if (!oldWidth && !oldHeight) {
      oldLeft = window.screen.availWidth / 2;
      oldTop = window.screen.availHeight / 2;
    }

    let appWindow = chrome.app.window.current();
    appWindow.innerBounds.width = Math.round(newWidth);
    appWindow.innerBounds.height = Math.round(newHeight);
    appWindow.outerBounds.left =
        Math.max(0, Math.round(oldLeft - (newWidth - oldWidth) / 2));
    appWindow.outerBounds.top =
        Math.max(0, Math.round(oldTop - (newHeight - oldHeight) / 2));
    appWindow.show();

    this.videoElement_.focus();
    this.videoElement_.play();
  }

  /**
   * Advance to next video when the current one ends.
   * Not using 'ended' event because dragging the timeline
   * thumb to the end when seeking will trigger 'ended' event.
   *
   * @private
   */
  onPause_() {
    this.videoElement_.loop = false;
    if (this.videoElement_.currentTime == this.videoElement_.duration) {
      this.advance_(true);
    }
  }

  /**
   * Advances to the next (or previous) track.
   *
   * @param {boolean} direction True to the next, false to the previous.
   * @private
   */
  advance_(direction) {
    let newPos = this.currentPos_ + (direction ? 1 : -1);
    if (newPos < 0 || newPos >= this.videos_.length) {
      return;
    }

    this.currentPos_ = newPos;
    this.reloadCurrentVideo_(() => {
      this.videoElement_.play();
    });
  }

  /**
   * Reloads the current video.
   *
   * @param {function()=} opt_callback Completion callback.
   */
  reloadCurrentVideo_(opt_callback) {
    let videoPlayerElement = getRequiredElement('video-player');
    if (this.currentPos_ == (this.videos_.length - 1)) {
      videoPlayerElement.setAttribute('last-video', true);
    } else {
      videoPlayerElement.removeAttribute('last-video');
    }

    if (this.currentPos_ === 0) {
      videoPlayerElement.setAttribute('first-video', true);
    } else {
      videoPlayerElement.removeAttribute('first-video');
    }

    let currentVideo = this.videos_[this.currentPos_];
    this.loadVideo_(currentVideo, opt_callback);
  }

  /**
   * Loads the video file.
   * @param {!FileEntry} video Entry of the video to be played.
   * @param {function()=} opt_callback Completion callback.
   * @private
   */
  async loadVideo_(video, opt_callback) {
    document.title = video.name;
    const videoUrl = video.toURL();

    if (opt_callback) {
      this.videoElement_.addEventListener(
          'loadedmetadata', opt_callback, {once: true});
    }

    this.videoElement_.src = videoUrl;

    const subtitleUrl = await this.searchSubtitle_(videoUrl);
    if (subtitleUrl) {
      const track =
          assertInstanceof(document.createElement('track'), HTMLTrackElement);
      track.src = subtitleUrl;
      track.kind = 'subtitles';
      track.default = true;
      this.videoElement_.appendChild(track);
    }
  }

  /**
   * Search subtitle file corresponding to a video.
   * @param {string} url a url of a video.
   * @return {Promise} a Promise returns url of subtitle file, or an empty
   *     string.
   */
  async searchSubtitle_(url) {
    const resolveLocalFileSystemWithExtension = (extension) => {
      const subtitleUrl = this.getSubtitleUrl_(url, extension);
      return new Promise(
          window.webkitResolveLocalFileSystemURL.bind(null, subtitleUrl));
    };

    try {
      const subtitle = await resolveLocalFileSystemWithExtension('.vtt');
      return subtitle.toURL();
    } catch (error) {
      // TODO: figure out if there could be any error other than
      // file not found or not accessible. If not, remove this log.
      console.error(error);
      return '';
    }
  }

  /**
   * Get the subtitle url.
   *
   * @private
   * @param {string} srcUrl Source url of the video file.
   * @param {string} extension Extension of the subtitle we are looking for.
   * @return {string} Subtitle url.
   */
  getSubtitleUrl_(srcUrl, extension) {
    return srcUrl.replace(/\.[^\.]+$/, extension);
  }
}

/**
 * 10 seconds should be skipped when J/L key is pressed.
 */
NativeControlsVideoPlayer.PROGRESS_MAX_SECONDS_TO_SKIP = 10;

/**
 * 20% of duration should be skipped when the video is too short to skip 10
 * seconds.
 */
NativeControlsVideoPlayer.PROGRESS_MAX_RATIO_TO_SKIP = 0.2;
