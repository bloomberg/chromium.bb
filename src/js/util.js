// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for utilities.
 */
camera.util = camera.util || {};

/**
 * Gets the clockwise rotation and flip that can orient a photo to its upright
 * position.
 * @param {Blob} blob JPEG blob that might contain EXIF orientation field.
 * @return {Promise<Object{rotation: number, flip: boolean}>}
 */
camera.util.getPhotoOrientation = function(blob) {
  let getOrientation = new Promise((resolve, reject) => {
    let reader = new FileReader();
    reader.onload = function(event) {
      let view = new DataView(event.target.result);
      if (view.getUint16(0, false) != 0xFFD8) {
        resolve(1);
        return;
      }
      let length = view.byteLength, offset = 2;
      while (offset < length) {
        if (view.getUint16(offset + 2, false) <= 8) {
          break;
        }
        let marker = view.getUint16(offset, false);
        offset += 2;
        if (marker == 0xFFE1) {
          if (view.getUint32(offset += 2, false) != 0x45786966) {
            break;
          }

          let little = view.getUint16(offset += 6, false) == 0x4949;
          offset += view.getUint32(offset + 4, little);
          let tags = view.getUint16(offset, little);
          offset += 2;
          for (let i = 0; i < tags; i++) {
            if (view.getUint16(offset + (i * 12), little) == 0x0112) {
              resolve(view.getUint16(offset + (i * 12) + 8, little));
              return;
            }
          }
        } else if ((marker & 0xFF00) != 0xFF00) {
          break;
        } else {
          offset += view.getUint16(offset, false);
        }
      }
      resolve(1);
    };
    reader.readAsArrayBuffer(blob);
  });

  return getOrientation.then(orientation => {
    switch (orientation) {
      case 1:
        return {rotation: 0, flip: false};
      case 2:
        return {rotation: 0, flip: true};
      case 3:
        return {rotation: 180, flip: false};
      case 4:
        return {rotation: 180, flip: true};
      case 5:
        return {rotation: 90, flip: true};
      case 6:
        return {rotation: 90, flip: false};
      case 7:
        return {rotation: 270, flip: true};
      case 8:
        return {rotation: 270, flip: false};
      default:
        return {rotation: 0, flip: false};
    }
  });
};

/**
 * Orients a photo to the upright orientation.
 * @param {Blob} blob Photo as a blob.
 * @param {function(Blob)} onSuccess Success callback with the result photo as
 *     a blob.
 * @param {function()} onFailure Failure callback.
 */
camera.util.orientPhoto = function(blob, onSuccess, onFailure) {
  // TODO(shenghao): Revise or remove this function if it's no longer
  // applicable.
  let drawPhoto = function(original, orientation, onSuccess, onFailure) {
    let canvas = document.createElement('canvas');
    let context = canvas.getContext('2d');
    let canvasSquareLength = Math.max(original.width, original.height);
    canvas.width = canvasSquareLength;
    canvas.height = canvasSquareLength;

    let centerX = canvas.width / 2, centerY = canvas.height / 2;
    context.translate(centerX, centerY);
    context.rotate(orientation.rotation * Math.PI / 180);
    if (orientation.flip) {
      context.scale(-1, 1);
    }
    context.drawImage(original, -original.width / 2, -original.height / 2,
        original.width, original.height);
    if (orientation.flip) {
      context.scale(-1, 1);
    }
    context.rotate(-orientation.rotation * Math.PI / 180);
    context.translate(-centerX, -centerY);

    let outputCanvas = document.createElement('canvas');
    if (orientation.rotation == 90 || orientation.rotation == 270) {
      outputCanvas.width = original.height;
      outputCanvas.height = original.width;
    } else {
      outputCanvas.width = original.width;
      outputCanvas.height = original.height;
    }
    let imageData = context.getImageData(
        (canvasSquareLength - outputCanvas.width) / 2,
        (canvasSquareLength - outputCanvas.height) / 2,
        outputCanvas.width, outputCanvas.height);
    let outputContext = outputCanvas.getContext('2d');
    outputContext.putImageData(imageData, 0, 0);

    outputCanvas.toBlob(function(blob) {
      if (blob) {
        onSuccess(blob);
      } else {
        onFailure();
      }
    }, 'image/jpeg');
  };

  camera.util.getPhotoOrientation(blob).then(orientation => {
    if (orientation.rotation == 0 && !orientation.flip) {
      onSuccess(blob);
    } else {
      let original = document.createElement('img');
      original.onload = function() {
        drawPhoto(original, orientation, onSuccess, onFailure);
      };
      original.onerror = onFailure;
      original.src = URL.createObjectURL(blob);
    }
  });
};

/**
 * Checks if the current device is in the given device list.
 * @param {Array.<string>} ids Device ids.
 * @return {!Promise<boolean>} Promise for the result.
 */
camera.util.isChromeOSDevice = function(ids) {
  return new Promise(resolve => {
    if (!chrome.chromeosInfoPrivate) {
      resolve(false);
      return;
    }
    chrome.chromeosInfoPrivate.get(['customizationId'], values => {
      var device = values['customizationId'];
      resolve(device && ids.indexOf(device) >= 0);
    });
  });
};

/**
 * Returns true if current installed Chrome version is larger than or equal to
 * the given version.
 * @param {number} minVersion the version to be compared with.
 * @return {boolean}
 */
camera.util.isChromeVersionAbove = function(minVersion) {
  var match = navigator.userAgent.match(/Chrom(e|ium)\/([0-9]+)\./);
  return (match ? parseInt(match[2], 10) : 0) >= minVersion;
};

/*
 * Checks if the user is using a Chrome OS device.
 * @return {boolean} Whether it is a Chrome OS device or not.
 */
camera.util.isChromeOS = function() {
  return navigator.appVersion.indexOf('CrOS') !== -1;
};

/**
 * Sets up localized aria attributes for TTS on the entire document. Uses the
 * dedicated i18n-aria-label attribute as a strings identifier. If it is not
 * found, then i18n-label is used as a fallback.
 */
camera.util.setupElementsAriaLabel = function() {
  var elements = document.querySelectorAll('*[i18n-aria-label], *[i18n-label]');
  for (var index = 0; index < elements.length; index++) {
    var label = elements[index].hasAttribute('i18n-aria-label') ?
        elements[index].getAttribute('i18n-aria-label') :
        elements[index].getAttribute('i18n-label');  // Fallback.

    elements[index].setAttribute('aria-label', chrome.i18n.getMessage(label));
  }
};

/**
 * Sets a class which invokes an animation and calls the callback when the
 * animation is done. The class is released once the animation is finished.
 * If the class name is already set, then calls onCompletion immediately.
 *
 * @param {HTMLElement} classElement Element to be applied the class on.
 * @param {HTMLElement} animationElement Element to be animated.
 * @param {string} className Class name to be added.
 * @param {number} timeout Animation timeout in milliseconds.
 * @param {function()=} opt_onCompletion Completion callback.
 */
camera.util.setAnimationClass = function(
    classElement, animationElement, className, timeout, opt_onCompletion) {
  if (classElement.classList.contains(className)) {
    if (opt_onCompletion)
      opt_onCompletion();
    return;
  }

  classElement.classList.add(className);
  var onAnimationCompleted = function() {
    classElement.classList.remove(className);
    if (opt_onCompletion)
      opt_onCompletion();
  };

  camera.util.waitForAnimationCompletion(
      animationElement, timeout, onAnimationCompleted);
};

/**
 * Waits for animation completion and calls the callback.
 *
 * @param {HTMLElement} animationElement Element to be animated.
 * @param {number} timeout Timeout for completion. 0 for no timeout.
 * @param {function()} onCompletion Completion callback.
 */
camera.util.waitForAnimationCompletion = function(
    animationElement, timeout, onCompletion) {
  var completed = false;
  var onAnimationCompleted = function(opt_event) {
    if (completed || (opt_event && opt_event.target != animationElement))
      return;
    completed = true;
    animationElement.removeEventListener(
        'webkitAnimationEnd', onAnimationCompleted);
    onCompletion();
  };
  if (timeout)
      setTimeout(onAnimationCompleted, timeout);
  animationElement.addEventListener('webkitAnimationEnd', onAnimationCompleted);
};

/**
 * Waits for transition completion and calls the callback.
 *
 * @param {HTMLElement} transitionElement Element to be transitioned.
 * @param {number} timeout Timeout for completion. 0 for no timeout.
 * @param {function()} onCompletion Completion callback.
 */
camera.util.waitForTransitionCompletion = function(
    transitionElement, timeout, onCompletion) {
  var completed = false;
  var onTransitionCompleted = function(opt_event) {
    if (completed || (opt_event && opt_event.target != transitionElement))
      return;
    completed = true;
    transitionElement.removeEventListener(
        'webkitTransitionEnd', onTransitionCompleted);
    onCompletion();
  };
  if (timeout)
      setTimeout(onTransitionCompleted, timeout);
  transitionElement.addEventListener(
      'webkitTransitionEnd', onTransitionCompleted);
};

/**
 * Scrolls the parent of the element so the element is visible.
 *
 * @param {HTMLElement} element Element to be visible.
 * @param {camera.util.SmoothScroller} scroller Scroller to be used.
 * @param {camera.util.SmoothScroller.Mode=} opt_mode Scrolling mode. Default:
 *     SMOOTH.
 */
camera.util.ensureVisible = function(element, scroller, opt_mode) {
  var scrollLeft = scroller.scrollLeft;
  var scrollTop = scroller.scrollTop;

  if (element.offsetTop < scroller.scrollTop)
    scrollTop = Math.round(element.offsetTop - element.offsetHeight * 0.5);
  if (element.offsetTop + element.offsetHeight >
      scrollTop + scroller.clientHeight) {
    scrollTop = Math.round(element.offsetTop + element.offsetHeight * 1.5 -
        scroller.clientHeight);
  }
  if (element.offsetLeft < scroller.scrollLeft)
    scrollLeft = Math.round(element.offsetLeft - element.offsetWidth * 0.5);
  if (element.offsetLeft + element.offsetWidth >
      scrollLeft + scroller.clientWidth) {
    scrollLeft = Math.round(element.offsetLeft + element.offsetWidth * 1.5 -
        scroller.clientWidth);
  }
  scroller.scrollTo(scrollLeft, scrollTop, opt_mode);
};

/**
 * Scrolls the parent of the element so the element is centered.
 *
 * @param {HTMLElement} element Element to be visible.
 * @param {camera.util.SmoothScroller} scroller Scroller to be used.
 * @param {camera.util.SmoothScroller.Mode=} opt_mode Scrolling mode. Default:
 *     SMOOTH.
 */
camera.util.scrollToCenter = function(element, scroller, opt_mode) {
  var scrollLeft = Math.round(element.offsetLeft + element.offsetWidth / 2 -
    scroller.clientWidth / 2);
  var scrollTop = Math.round(element.offsetTop + element.offsetHeight / 2 -
    scroller.clientHeight / 2);

  scroller.scrollTo(scrollLeft, scrollTop, opt_mode);
};

/**
 * Wraps an effect in style implemented as either CSS3 animation or CSS3
 * transition. The passed closure should invoke the effect.
 * Only the last callback, passed to the latest invoke() call will be called
 * on the transition or the animation end.
 *
 * @param {function(*, function())} closure Closure for invoking the effect.
 * @constructor
 */
camera.util.StyleEffect = function(closure) {
  /**
   * @type {function(*, function()}
   * @private
   */
  this.closure_ = closure;

  /**
   * Callback to be called for the latest invokation.
   * @type {?function()}
   * @private
   */
  this.callback_ = null;

  /**
   * @type {?number{
   * @private
   */
  this.invocationTimer_ = null;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.util.StyleEffect.prototype = {
  get animating() {
    return !!this.callback_;
  }
};

/**
 * Invokes the animation and calls the callback on completion. Note, that
 * the callback will not be called if there is another invocation called after.
 *
 * @param {*} state State of the effect to be set
 * @param {function()} callback Completion callback.
 * @param {number=} opt_delay Timeout in milliseconds before invoking.
 */
camera.util.StyleEffect.prototype.invoke = function(
    state, callback, opt_delay) {
  if (this.invocationTimer_) {
    clearTimeout(this.invocationTimer_);
    this.invocationTimer_ = null;
  }

  var invokeClosure = function() {
    this.callback_ = callback;
    this.closure_(state, function() {
      if (!this.callback_)
        return;
      var callback = this.callback_;
      this.callback_ = null;

      // Let the animation neatly finish.
      setTimeout(callback, 0);
    }.bind(this));
  }.bind(this);

  if (opt_delay !== undefined)
    this.invocationTimer_ = setTimeout(invokeClosure, opt_delay);
  else
    invokeClosure();
};

/**
 * Performs smooth scrolling of a scrollable DOM element using a accelerated
 * CSS3 transform and transition for smooth animation.
 *
 * @param {HTMLElement} element Element to be scrolled.
 * @param {HTMLElement} padder Element holding contents within the scrollable
 *     element.
 * @constructor
 */
camera.util.SmoothScroller = function(element, padder) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.padder_ = padder;

  /**
   * @type {boolean}
   * @private
   */
  this.animating_ = false;

  /**
   * @type {number}
   * @private
   */
  this.animationId_ = 0;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Smooth scrolling animation duration in milliseconds.
 * @type {number}
 * @const
 */
camera.util.SmoothScroller.DURATION = 500;

/**
 * Mode of scrolling.
 * @enum {number}
 */
camera.util.SmoothScroller.Mode = {
  SMOOTH: 0,
  INSTANT: 1
};

camera.util.SmoothScroller.prototype = {
  get element() {
    return this.element_;
  },
  get animating() {
    return this.animating_;
  },
  get scrollLeft() {
    // TODO(mtomasz): This does not reflect paddings nor margins.
    return -this.padder_.getBoundingClientRect().left;
  },
  get scrollTop() {
    // TODO(mtomasz): This does not reflect paddings nor margins.
    return -this.padder_.getBoundingClientRect().top;
  },
  get scrollWidth() {
    // TODO(mtomasz): This does not reflect paddings nor margins.
    return this.padder_.scrollWidth;
  },
  get scrollHeight() {
    // TODO(mtomasz): This does not reflect paddings nor margins.
    return this.padder_.scrollHeight;
  },
  get clientWidth() {
    // TODO(mtomasz): This does not reflect paddings nor margins.
    return this.element_.clientWidth;
  },
  get clientHeight() {
    // TODO(mtomasz): This does not reflect paddings nor margins.
    return this.element_.clientHeight;
  }
};

/**
 * Flushes the CSS3 transition scroll to real scrollLeft/scrollTop attributes.
 * @private
 */
camera.util.SmoothScroller.prototype.flushScroll_ = function() {
  var scrollLeft = this.scrollLeft;
  var scrollTop = this.scrollTop;

  this.padder_.style.transition = '';
  this.padder_.style.webkitTransform = '';

  this.element_.scrollLeft = scrollLeft;
  this.element_.scrollTop = scrollTop;

  this.animationId_++;  // Invalidate the animation by increasing the id.
  this.animating_ = false;
};

/**
 * Scrolls smoothly to specified position.
 *
 * @param {number} x X Target scrollLeft value.
 * @param {number} y Y Target scrollTop value.
 * @param {camera.util.SmoothScroller.Mode=} opt_mode Scrolling mode. Default:
 *     SMOOTH.
 */
camera.util.SmoothScroller.prototype.scrollTo = function(x, y, opt_mode) {
  var mode = opt_mode || camera.util.SmoothScroller.Mode.SMOOTH;

  // Limit to the allowed values.
  var x = Math.max(0, Math.min(x, this.scrollWidth - this.clientWidth));
  var y = Math.max(0, Math.min(y, this.scrollHeight - this.clientHeight));

  switch (mode) {
    case camera.util.SmoothScroller.Mode.INSTANT:
      // Cancel any current animations.
      if (this.animating_)
        this.flushScroll_();

      this.element_.scrollLeft = x;
      this.element_.scrollTop = y;
      break;

    case camera.util.SmoothScroller.Mode.SMOOTH:
      // Calculate translating offset using the accelerated CSS3 transform.
      var dx = Math.round(x - this.element_.scrollLeft);
      var dy = Math.round(y - this.element_.scrollTop);

      var transformString =
          'translate(' + -dx + 'px, ' + -dy + 'px)';

      // If nothing to change, then return.
      if (this.padder_.style.webkitTransform == transformString ||
          (dx == 0 && dy == 0 && !this.padder_.style.webkitTransform)) {
        return;
      }

      // Invalidate previous invocations.
      var currentAnimationId = ++this.animationId_;

      // Start the accelerated animation.
      this.animating_ = true;
      this.padder_.style.transition = '-webkit-transform ' +
          camera.util.SmoothScroller.DURATION + 'ms ease-out';
      this.padder_.style.webkitTransform = transformString;

      // Remove translation, and switch to scrollLeft/scrollTop when the
      // animation is finished.
      camera.util.waitForTransitionCompletion(
          this.padder_,
          0,
          function() {
            // Check if the animation got invalidated by a later scroll.
            if (currentAnimationId == this.animationId_)
              this.flushScroll_();
         }.bind(this));
      break;
  }
};

/**
 * Tracks the mouse for click and move, and the touch screen for touches. If
 * any of these are detected, then the callback is called.
 *
 * @param {HTMLElement} element Element to be monitored.
 * @param {function(Event)} callback Callback triggered on events detected.
 * @constructor
 */
camera.util.PointerTracker = function(element, callback) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {function(Event)}
   * @private
   */
  this.callback_ = callback;

  /**
   * @type {Array.<number>}
   * @private
   */
  this.lastMousePosition_ = null;

  // End of properties. Seal the object.
  Object.seal(this);

  // Add the event listeners.
  this.element_.addEventListener('mousedown', this.onMouseDown_.bind(this));
  this.element_.addEventListener('mousemove', this.onMouseMove_.bind(this));
  this.element_.addEventListener('touchstart', this.onTouchStart_.bind(this));
  this.element_.addEventListener('touchmove', this.onTouchMove_.bind(this));
};

/**
 * Handles the mouse down event.
 *
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.PointerTracker.prototype.onMouseDown_ = function(event) {
  this.callback_(event);
  this.lastMousePosition_ = [event.screenX, event.screenY];
};

/**
 * Handles the mouse move event.
 *
 * @param {Event} event Mouse move event.
 * @private
 */
camera.util.PointerTracker.prototype.onMouseMove_ = function(event) {
  // Ignore mouse events, which are invoked on the same position, but with
  // changed client coordinates. This will happen eg. when scrolling. We should
  // ignore them, since they are not invoked by an actual mouse move.
  if (this.lastMousePosition_ && this.lastMousePosition_[0] == event.screenX &&
      this.lastMousePosition_[1] == event.screenY) {
    return;
  }

  this.callback_(event);
  this.lastMousePosition_ = [event.screenX, event.screenY];
};

/**
 * Handles the touch start event.
 *
 * @param {Event} event Touch start event.
 * @private
 */
camera.util.PointerTracker.prototype.onTouchStart_ = function(event) {
  this.callback_(event);
};

/**
 * Handles the touch move event.
 *
 * @param {Event} event Touch move event.
 * @private
 */
camera.util.PointerTracker.prototype.onTouchMove_ = function(event) {
  this.callback_(event);
};

/**
 * Tracks scrolling and calls a callback, when scrolling is started and ended
 * by either the scroller or the user.
 *
 * @param {camera.util.SmoothScroller} scroller Scroller object to be tracked.
 * @param {function()} onScrollStarted Callback called when scrolling is
 *     started.
 * @param {function()} onScrollEnded Callback called when scrolling is ended.
 * @constructor
 */
camera.util.ScrollTracker = function(scroller, onScrollStarted, onScrollEnded) {
  /**
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = scroller;

  /**
   * @type {function()}
   * @private
   */
  this.onScrollStarted_ = onScrollStarted;

  /**
   * @type {function()}
   * @private
   */
  this.onScrollEnded_ = onScrollEnded;

  /**
   * Timer to probe for scroll changes, every 100 ms.
   * @type {?number}
   * @private
   */
  this.timer_ = null;

  /**
   * Workaround for: crbug.com/135780.
   * @type {?number}
   * @private
   */
  this.noChangeTimer_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.scrolling_ = false;

  /**
   * @type {Array.<number>}
   * @private
   */
  this.startScrollPosition_ = [0, 0];

  /**
   * @type {Array.<number>}
   * @private
   */
  this.lastScrollPosition_ = [0, 0];

  /**
   * Whether the touch screen is currently touched.
   * @type {boolean}
   * @private
   */
  this.touchPressed_ = false;

  /**
   * Whether the touch screen is currently touched.
   * @type {boolean}
   * @private
   */
  this.mousePressed_ = false;

  // End of properties. Seal the object.
  Object.seal(this);

  // Register event handlers.
  this.scroller_.element.addEventListener(
      'mousedown', this.onMouseDown_.bind(this));
  this.scroller_.element.addEventListener(
      'touchstart', this.onTouchStart_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
  window.addEventListener('touchend', this.onTouchEnd_.bind(this));
};

camera.util.ScrollTracker.prototype = {
  /**
   * @return {boolean} Whether scrolling is being performed or not.
   */
  get scrolling() {
    return this.scrolling_;
  },

  /**
   * @return {Array.<number>} Returns distance of the last detected scroll.
   */
  get delta() {
    return [
      this.startScrollPosition_[0] - this.lastScrollPosition_[0],
      this.startScrollPosition_[1] - this.lastScrollPosition_[1]
    ];
  }
};

/**
 * Handles pressing the mouse button.
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.ScrollTracker.prototype.onMouseDown_ = function(event) {
  this.mousePressed_ = true;
};

/**
 * Handles releasing the mouse button.
 * @param {Event} event Mouse up event.
 * @private
 */
camera.util.ScrollTracker.prototype.onMouseUp_ = function(event) {
  this.mousePressed_ = false;
};

/**
 * Handles touching the screen.
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.ScrollTracker.prototype.onTouchStart_ = function(event) {
  this.touchPressed_ = true;
};

/**
 * Handles releasing touching of the screen.
 * @param {Event} event Mouse up event.
 * @private
 */
camera.util.ScrollTracker.prototype.onTouchEnd_ = function(event) {
  this.touchPressed_ = false;
};

/**
 * Starts monitoring.
 */
camera.util.ScrollTracker.prototype.start = function() {
  if (this.timer_ !== null)
    return;
  this.timer_ = setInterval(this.probe_.bind(this), 100);
};

/**
 * Stops monitoring.
 */
camera.util.ScrollTracker.prototype.stop = function() {
  if (this.timer_ === null)
    return;
  clearTimeout(this.timer_);
  this.timer_ = null;
};

/**
 * Probes for scrolling changes.
 * @private
 */
camera.util.ScrollTracker.prototype.probe_ = function() {
  var scrollLeft = this.scroller_.scrollLeft;
  var scrollTop = this.scroller_.scrollTop;

  var scrollChanged =
      scrollLeft != this.lastScrollPosition_[0] ||
      scrollTop != this.lastScrollPosition_[1] ||
      this.scroller_.animating;

  if (scrollChanged) {
    if (!this.scrolling_) {
      this.startScrollPosition_ = [scrollLeft, scrollTop];
      this.onScrollStarted_();
    }
    this.scrolling_ = true;
  } else {
    if (!this.mousePressed_ && !this.touchPressed_ && this.scrolling_) {
      this.onScrollEnded_();
      this.scrolling_ = false;
    }
  }

  // Workaround for: crbug.com/135780.
  // When scrolling by touch screen, the touchend event is not emitted. So, a
  // timer has to be used as a fallback to detect the end of scrolling.
  if (this.touchPressed_) {
    if (scrollChanged) {
      // Scrolling changed, cancel the timer.
      if (this.noChangeTimer_) {
        clearTimeout(this.noChangeTimer_);
        this.noChangeTimer_ = null;
      }
    } else {
      // Scrolling previously, but now no change is detected, so set a timer.
      if (this.scrolling_ && !this.noChangeTimer_) {
        this.noChangeTimer_ = setTimeout(function() {
          this.onScrollEnded_();
          this.scrolling_ = false;
          this.touchPressed_ = false;
          this.noChangeTimer_ = null;
        }.bind(this), 200);
      }
    }
  }

  this.lastScrollPosition_ = [scrollLeft, scrollTop];
};

/**
 * Makes an element scrollable by dragging with a mouse.
 *
 * @param {camera.util.Scroller} scroller Scroller for the element.
 * @constructor
 */
camera.util.MouseScroller = function(scroller) {
  /**
   * @type {camera.util.Scroller}
   * @private
   */
  this.scroller_ = scroller;

  /**
   * @type {Array.<number>}
   * @private
   */
  this.startPosition_ = null;

  /**
   * @type {Array.<number>}
   * @private
   */
  this.startScrollPosition_ = null;

  // End of properties. Seal the object.
  Object.seal(this);

  // Register mouse handlers.
  this.scroller_.element.addEventListener(
      'mousedown', this.onMouseDown_.bind(this));
  window.addEventListener('mousemove', this.onMouseMove_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
};

/**
 * Handles the mouse down event on the tracked element.
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.MouseScroller.prototype.onMouseDown_ = function(event) {
  if (event.which != 1)
    return;

  this.startPosition_ = [event.screenX, event.screenY];
  this.startScrollPosition_ = [
    this.scroller_.scrollLeft,
    this.scroller_.scrollTop
  ];
};

/**
 * Handles moving a mouse over the tracker element.
 * @param {Event} event Mouse move event.
 * @private
 */
camera.util.MouseScroller.prototype.onMouseMove_ = function(event) {
  if (!this.startPosition_)
    return;

  // It may happen that we won't receive the mouseup event, when clicking on
  // the -webkit-app-region: drag area.
  if (event.which != 1) {
    this.startPosition_ = null;
    this.startScrollPosition_ = null;
    return;
  }

  var scrollLeft =
      this.startScrollPosition_[0] - (event.screenX - this.startPosition_[0]);
  var scrollTop =
      this.startScrollPosition_[1] - (event.screenY - this.startPosition_[1]);

  this.scroller_.scrollTo(
      scrollLeft, scrollTop, camera.util.SmoothScroller.Mode.INSTANT);
};

/**
 * Handles the mouse up event on the tracked element.
 * @param {Event} event Mouse down event.
 * @private
 */
camera.util.MouseScroller.prototype.onMouseUp_ = function(event) {
  this.startPosition_ = null;
  this.startScrollPosition_ = null;
};

/**
 * Returns a shortcut string, such as Ctrl-Alt-A.
 * @param {Event} event Keyboard event.
 * @return {string} Shortcut identifier.
 */
camera.util.getShortcutIdentifier = function(event) {
  var identifier = (event.ctrlKey ? 'Ctrl-' : '') +
                   (event.altKey ? 'Alt-' : '') +
                   (event.shiftKey ? 'Shift-' : '') +
                   (event.metaKey ? 'Meta-' : '');
  if (event.key) {
    switch (event.key) {
      case 'ArrowLeft':
        identifier += 'Left';
        break;
      case 'ArrowRight':
        identifier += 'Right';
        break;
      case 'ArrowDown':
        identifier += 'Down';
        break;
      case 'ArrowUp':
        identifier += 'Up';
        break;
      case 'a':
        identifier += 'A';
        break;
      case 'p':
        identifier += 'P';
        break;
      case 's':
        identifier += 'S';
        break;
      default:
        identifier += event.key;
    }
  }
  return identifier;
};

/**
 * Makes all elements with a tabindex attribute unfocusable by mouse.
 */
camera.util.makeElementsUnfocusableByMouse = function() {
  var elements = document.querySelectorAll('[tabindex]');
  for (var index = 0; index < elements.length; index++) {
    elements[index].addEventListener('mousedown', function(event) {
      event.preventDefault();
    });
  }
};

/**
 * Updates the wrapped element size according to the given bounds. The wrapped
 * content (either img or video child element) should keep the aspect ratio and
 * is either filled up or letterboxed inside the wrapper element.
 * @param {HTMLElement} wrapper Element whose wrapped child to be updated.
 * @param {number} boundWidth Bound width in pixels.
 * @param {number} boundHeight Bound height in pixels.
 * @param {boolean} fill True to fill up and crop the content to the bounds,
 *     false to letterbox the content within the bounds.
 */
camera.util.updateElementSize = function(
    wrapper, boundWidth, boundHeight, fill) {
  // Assume the wrapped child is either img or video element.
  var child = wrapper.firstElementChild;
  var srcWidth = child.naturalWidth || child.videoWidth;
  var srcHeight = child.naturalHeight || child.videoHeight;
  var f = fill ? Math.max : Math.min;
  var scale = f(boundHeight / srcHeight, boundWidth / srcWidth);

  // Corresponding CSS should handle the adjusted sizes for proper display.
  child.width = Math.round(scale * srcWidth);
  child.height = Math.round(scale * srcHeight);
};

/*
 * Checks if the window is maximized or fullscreen.
 * @return {boolean} True if maximized or fullscreen, false otherwise.
 */
camera.util.isWindowFullSize = function() {
  // App-window's isFullscreen state and window's outer-size may not be updated
  // immediately during resizing. Use app-window's isMaximized state and
  // window's inner-size here as workarounds.
  var fullscreen = window.innerWidth >= screen.width &&
      window.innerHeight >= screen.height;
  return chrome.app.window.current().isMaximized() || fullscreen;
};
