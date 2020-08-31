// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base class to represent a "view". A view is an absolutely positioned box on
 * the page.
 */
const View = (function() {
  'use strict';

  /**
   * @constructor
   */
  function View() {
    this.isVisible_ = true;
  }

  View.prototype = {
    /**
     * Called to reposition the view on the page. Measurements are in pixels.
     */
    setGeometry(left, top, width, height) {
      this.left_ = left;
      this.top_ = top;
      this.width_ = width;
      this.height_ = height;
    },

    /**
     * Called to show/hide the view.
     */
    show(isVisible) {
      this.isVisible_ = isVisible;
    },

    isVisible() {
      return this.isVisible_;
    },

    /**
     * Method of the observer class.
     *
     * Called to check if an observer needs the data it is
     * observing to be actively updated.
     */
    isActive() {
      return this.isVisible();
    },

    getLeft() {
      return this.left_;
    },

    getTop() {
      return this.top_;
    },

    getWidth() {
      return this.width_;
    },

    getHeight() {
      return this.height_;
    },

    getRight() {
      return this.getLeft() + this.getWidth();
    },

    getBottom() {
      return this.getTop() + this.getHeight();
    },

    setParameters(params) {},

    /**
     * Called when loading a log file, after clearing all events, but before
     * loading the new ones.  |polledData| contains the data from all
     * PollableData helpers.  |tabData| contains the data for the particular
     * tab.  |logDump| is the entire log dump, which includes the other two
     * values.  It's included separately so most views don't have to depend on
     * its specifics.
     */
    onLoadLogStart(polledData, tabData, logDump) {},

    /**
     * Called as the final step of loading a log file.  Arguments are the same
     * as onLoadLogStart.  Returns true to indicate the tab should be shown,
     * false otherwise.
     */
    onLoadLogFinish(polledData, tabData, logDump) {
      return false;
    }
  };

  return View;
})();

//-----------------------------------------------------------------------------

/**
 * DivView is an implementation of View that wraps a DIV.
 */
const DivView = (function() {
  'use strict';

  // We inherit from View.
  const superClass = View;

  /**
   * @constructor
   */
  function DivView(divId) {
    // Call superclass's constructor.
    superClass.call(this);

    this.node_ = $(divId);
    if (!this.node_) {
      throw new Error('Element ' + divId + ' not found');
    }

    // Initialize the default values to those of the DIV.
    this.width_ = this.node_.offsetWidth;
    this.height_ = this.node_.offsetHeight;
    this.isVisible_ = this.node_.style.display != 'none';
  }

  DivView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);

      this.node_.style.position = 'absolute';
      setNodePosition(this.node_, left, top, width, height);
    },

    show(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      setNodeDisplay(this.node_, isVisible);
    },

    /**
     * Returns the wrapped DIV
     */
    getNode() {
      return this.node_;
    }
  };

  return DivView;
})();


//-----------------------------------------------------------------------------

/**
 * Implementation of View that sizes its child to fit the entire window.
 *
 * @param {!View} childView The child view.
 */
const WindowView = (function() {
  'use strict';

  // We inherit from View.
  const superClass = View;

  /**
   * @constructor
   */
  function WindowView(childView) {
    // Call superclass's constructor.
    superClass.call(this);

    this.childView_ = childView;
    window.addEventListener('resize', this.resetGeometry.bind(this), true);
  }

  WindowView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);
      this.childView_.setGeometry(left, top, width, height);
    },

    show() {
      superClass.prototype.show.call(this, isVisible);
      this.childView_.show(isVisible);
    },

    resetGeometry() {
      this.setGeometry(
          0, 0, document.documentElement.clientWidth,
          document.documentElement.clientHeight);
    }
  };

  return WindowView;
})();

/**
 * View that positions two views vertically. The top view should be
 * fixed-height, and the bottom view will fill the remainder of the space.
 *
 *  +-----------------------------------+
 *  |            topView                |
 *  +-----------------------------------+
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  |          bottomView               |
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  +-----------------------------------+
 */
const VerticalSplitView = (function() {
  'use strict';

  // We inherit from View.
  const superClass = View;

  /**
   * @param {!View} topView The top view.
   * @param {!View} bottomView The bottom view.
   * @constructor
   */
  function VerticalSplitView(topView, bottomView) {
    // Call superclass's constructor.
    superClass.call(this);

    this.topView_ = topView;
    this.bottomView_ = bottomView;
  }

  VerticalSplitView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);

      const fixedHeight = this.topView_.getHeight();
      this.topView_.setGeometry(left, top, width, fixedHeight);

      this.bottomView_.setGeometry(
          left, top + fixedHeight, width, height - fixedHeight);
    },

    show(isVisible) {
      superClass.prototype.show.call(this, isVisible);

      this.topView_.show(isVisible);
      this.bottomView_.show(isVisible);
    }
  };

  return VerticalSplitView;
})();

/**
 * View that positions two views horizontally. The left view should be
 * fixed-width, and the right view will fill the remainder of the space.
 *
 *  +----------+--------------------------+
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  | leftView |       rightView          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  |          |                          |
 *  +----------+--------------------------+
 */
const HorizontalSplitView = (function() {
  'use strict';

  // We inherit from View.
  const superClass = View;

  /**
   * @param {!View} leftView The left view.
   * @param {!View} rightView The right view.
   * @constructor
   */
  function HorizontalSplitView(leftView, rightView) {
    // Call superclass's constructor.
    superClass.call(this);

    this.leftView_ = leftView;
    this.rightView_ = rightView;
  }

  HorizontalSplitView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);

      const fixedWidth = this.leftView_.getWidth();
      this.leftView_.setGeometry(left, top, fixedWidth, height);

      this.rightView_.setGeometry(
          left + fixedWidth, top, width - fixedWidth, height);
    },

    show(isVisible) {
      superClass.prototype.show.call(this, isVisible);

      this.leftView_.show(isVisible);
      this.rightView_.show(isVisible);
    }
  };

  return HorizontalSplitView;
})();
