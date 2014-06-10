// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Crop mode.
 * @constructor
 */
ImageEditor.Mode.Crop = function() {
  ImageEditor.Mode.call(this, 'crop', 'GALLERY_CROP');
};

ImageEditor.Mode.Crop.prototype = {__proto__: ImageEditor.Mode.prototype};

/**
 * TODO(JSDOC).
 */
ImageEditor.Mode.Crop.prototype.setUp = function() {
  ImageEditor.Mode.prototype.setUp.apply(this, arguments);

  var container = this.getImageView().container_;
  var doc = container.ownerDocument;

  this.domOverlay_ = doc.createElement('div');
  this.domOverlay_.className = 'crop-overlay';
  container.appendChild(this.domOverlay_);

  this.shadowTop_ = doc.createElement('div');
  this.shadowTop_.className = 'shadow';
  this.domOverlay_.appendChild(this.shadowTop_);

  this.middleBox_ = doc.createElement('div');
  this.middleBox_.className = 'middle-box';
  this.domOverlay_.appendChild(this.middleBox_);

  this.shadowLeft_ = doc.createElement('div');
  this.shadowLeft_.className = 'shadow';
  this.middleBox_.appendChild(this.shadowLeft_);

  this.cropFrame_ = doc.createElement('div');
  this.cropFrame_.className = 'crop-frame';
  this.middleBox_.appendChild(this.cropFrame_);

  this.shadowRight_ = doc.createElement('div');
  this.shadowRight_.className = 'shadow';
  this.middleBox_.appendChild(this.shadowRight_);

  this.shadowBottom_ = doc.createElement('div');
  this.shadowBottom_.className = 'shadow';
  this.domOverlay_.appendChild(this.shadowBottom_);

  var cropFrame = this.cropFrame_;
  function addCropFrame(className) {
    var div = doc.createElement('div');
    div.className = className;
    cropFrame.appendChild(div);
  }

  addCropFrame('left top corner');
  addCropFrame('top horizontal');
  addCropFrame('right top corner');
  addCropFrame('left vertical');
  addCropFrame('right vertical');
  addCropFrame('left bottom corner');
  addCropFrame('bottom horizontal');
  addCropFrame('right bottom corner');

  this.onResizedBound_ = this.onResized_.bind(this);
  window.addEventListener('resize', this.onResizedBound_);

  this.createDefaultCrop();
};

/**
 * Handles resizing of the window and updates the crop rectangle.
 * @private
 */
ImageEditor.Mode.Crop.prototype.onResized_ = function() {
  this.positionDOM();
};

/**
 * TODO(JSDOC).
 */
ImageEditor.Mode.Crop.prototype.reset = function() {
  ImageEditor.Mode.prototype.reset.call(this);
  this.createDefaultCrop();
};

/**
 * TODO(JSDOC).
 */
ImageEditor.Mode.Crop.prototype.positionDOM = function() {
  var screenClipped = this.viewport_.getScreenClipped();

  var screenCrop = this.viewport_.imageToScreenRect(this.cropRect_.getRect());
  var delta = ImageEditor.Mode.Crop.MOUSE_GRAB_RADIUS;
  this.editor_.hideOverlappingTools(
      screenCrop.inflate(delta, delta),
      screenCrop.inflate(-delta, -delta));

  this.domOverlay_.style.left = screenClipped.left + 'px';
  this.domOverlay_.style.top = screenClipped.top + 'px';
  this.domOverlay_.style.width = screenClipped.width + 'px';
  this.domOverlay_.style.height = screenClipped.height + 'px';

  this.shadowLeft_.style.width = screenCrop.left - screenClipped.left + 'px';

  this.shadowTop_.style.height = screenCrop.top - screenClipped.top + 'px';

  this.shadowRight_.style.width = screenClipped.left + screenClipped.width -
      (screenCrop.left + screenCrop.width) + 'px';

  this.shadowBottom_.style.height = screenClipped.top + screenClipped.height -
      (screenCrop.top + screenCrop.height) + 'px';
};

/**
 * TODO(JSDOC).
 */
ImageEditor.Mode.Crop.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  this.domOverlay_.parentNode.removeChild(this.domOverlay_);
  this.domOverlay_ = null;
  this.editor_.hideOverlappingTools();
  window.removeEventListener(this.onResizedBound_);
  this.onResizedBound_ = null;
};

/**
 * @const
 * @type {number}
 */
ImageEditor.Mode.Crop.MOUSE_GRAB_RADIUS = 6;
/**
 * @const
 * @type {number}
 */
ImageEditor.Mode.Crop.TOUCH_GRAB_RADIUS = 20;

/**
 * TODO(JSDOC).
 * @return {Command.Crop}  // TODO(JSDOC).
 */
ImageEditor.Mode.Crop.prototype.getCommand = function() {
  var cropImageRect = this.cropRect_.getRect();
  return new Command.Crop(cropImageRect);
};

/**
 * TODO(JSDOC).
 */
ImageEditor.Mode.Crop.prototype.createDefaultCrop = function() {
  var rect = new Rect(this.getViewport().getImageClipped());
  rect = rect.inflate(
      -Math.round(rect.width / 6), -Math.round(rect.height / 6));
  this.cropRect_ = new DraggableRect(rect, this.getViewport());
  this.positionDOM();
};

/**
 * TODO(JSDOC).
 * @param {number} x X coordinate for cursor.
 * @param {number} y Y coordinate for cursor.
 * @param {boolean} mouseDown If mouse button is down.
 * @return {string} A value for style.cursor CSS property.
 */
ImageEditor.Mode.Crop.prototype.getCursorStyle = function(x, y, mouseDown) {
  return this.cropRect_.getCursorStyle(x, y, mouseDown);
};

/**
 * TODO(JSDOC).
 * @param {number} x Event X coordinate.
 * @param {number} y Event Y coordinate.
 * @param {boolean} touch True if it's a touch event, false if mouse.
 * @return {function(number,number)} A function to be called on mouse drag.
 */
ImageEditor.Mode.Crop.prototype.getDragHandler = function(x, y, touch) {
  var cropDragHandler = this.cropRect_.getDragHandler(x, y, touch);
  if (!cropDragHandler) return null;

  var self = this;
  return function(x, y) {
    cropDragHandler(x, y);
    self.markUpdated();
    self.positionDOM();
  };
};

/**
 * TODO(JSDOC).
 * @param {number} x X coordinate of the event.
 * @param {number} y Y coordinate of the event.
 * @return {ImageBuffer.DoubleTapAction} Action to perform as result.
 */
ImageEditor.Mode.Crop.prototype.getDoubleTapAction = function(x, y) {
  return this.cropRect_.getDoubleTapAction(x, y);
};

/*
 * A draggable rectangle over the image.
 * @param {Rect} rect  // TODO(JSDOC).
 * @param {Viewport} viewport  // TODO(JSDOC).
 * @constructor
 */
function DraggableRect(rect, viewport) {
  // The bounds are not held in a regular rectangle (with width/height).
  // left/top/right/bottom held instead for convenience.
  this.bounds_ = {};
  this.bounds_[DraggableRect.LEFT] = rect.left;
  this.bounds_[DraggableRect.RIGHT] = rect.left + rect.width;
  this.bounds_[DraggableRect.TOP] = rect.top;
  this.bounds_[DraggableRect.BOTTOM] = rect.top + rect.height;

  this.viewport_ = viewport;

  this.oppositeSide_ = {};
  this.oppositeSide_[DraggableRect.LEFT] = DraggableRect.RIGHT;
  this.oppositeSide_[DraggableRect.RIGHT] = DraggableRect.LEFT;
  this.oppositeSide_[DraggableRect.TOP] = DraggableRect.BOTTOM;
  this.oppositeSide_[DraggableRect.BOTTOM] = DraggableRect.TOP;

  // Translation table to form CSS-compatible cursor style.
  this.cssSide_ = {};
  this.cssSide_[DraggableRect.LEFT] = 'w';
  this.cssSide_[DraggableRect.TOP] = 'n';
  this.cssSide_[DraggableRect.RIGHT] = 'e';
  this.cssSide_[DraggableRect.BOTTOM] = 's';
  this.cssSide_[DraggableRect.NONE] = '';
}

// Static members to simplify reflective access to the bounds.
/**
 * @const
 * @type {string}
 */
DraggableRect.LEFT = 'left';
/**
 * @const
 * @type {string}
 */
DraggableRect.RIGHT = 'right';
/**
 * @const
 * @type {string}
 */
DraggableRect.TOP = 'top';
/**
 * @const
 * @type {string}
 */
DraggableRect.BOTTOM = 'bottom';
/**
 * @const
 * @type {string}
 */
DraggableRect.NONE = 'none';

/**
 * TODO(JSDOC)
 * @return {number}  // TODO(JSDOC).
 */
DraggableRect.prototype.getLeft = function() {
  return this.bounds_[DraggableRect.LEFT];
};

/**
 * TODO(JSDOC)
 * @return {number}  // TODO(JSDOC).
 */
DraggableRect.prototype.getRight = function() {
  return this.bounds_[DraggableRect.RIGHT];
};

/**
 * TODO(JSDOC)
 * @return {number}  // TODO(JSDOC).
 */
DraggableRect.prototype.getTop = function() {
  return this.bounds_[DraggableRect.TOP];
};

/**
 * TODO(JSDOC)
 * @return {number}  // TODO(JSDOC).
 */
DraggableRect.prototype.getBottom = function() {
  return this.bounds_[DraggableRect.BOTTOM];
};

/**
 * TODO(JSDOC)
 * @return {Rect}  // TODO(JSDOC).
 */
DraggableRect.prototype.getRect = function() {
  return new Rect(this.bounds_);
};

/**
 * TODO(JSDOC)
 * @param {number} x X coordinate for cursor.
 * @param {number} y Y coordinate for cursor.
 * @param {boolean} touch  // TODO(JSDOC).
 * @return {Object}  // TODO(JSDOC).
 */
DraggableRect.prototype.getDragMode = function(x, y, touch) {
  var result = {
    xSide: DraggableRect.NONE,
    ySide: DraggableRect.NONE
  };

  var bounds = this.bounds_;
  var R = this.viewport_.screenToImageSize(
      touch ? ImageEditor.Mode.Crop.TOUCH_GRAB_RADIUS :
              ImageEditor.Mode.Crop.MOUSE_GRAB_RADIUS);

  var circle = new Circle(x, y, R);

  var xBetween = ImageUtil.between(bounds.left, x, bounds.right);
  var yBetween = ImageUtil.between(bounds.top, y, bounds.bottom);

  if (circle.inside(bounds.left, bounds.top)) {
    result.xSide = DraggableRect.LEFT;
    result.ySide = DraggableRect.TOP;
  } else if (circle.inside(bounds.left, bounds.bottom)) {
    result.xSide = DraggableRect.LEFT;
    result.ySide = DraggableRect.BOTTOM;
  } else if (circle.inside(bounds.right, bounds.top)) {
    result.xSide = DraggableRect.RIGHT;
    result.ySide = DraggableRect.TOP;
  } else if (circle.inside(bounds.right, bounds.bottom)) {
    result.xSide = DraggableRect.RIGHT;
    result.ySide = DraggableRect.BOTTOM;
  } else if (yBetween && Math.abs(x - bounds.left) <= R) {
    result.xSide = DraggableRect.LEFT;
  } else if (yBetween && Math.abs(x - bounds.right) <= R) {
    result.xSide = DraggableRect.RIGHT;
  } else if (xBetween && Math.abs(y - bounds.top) <= R) {
    result.ySide = DraggableRect.TOP;
  } else if (xBetween && Math.abs(y - bounds.bottom) <= R) {
    result.ySide = DraggableRect.BOTTOM;
  } else if (xBetween && yBetween) {
    result.whole = true;
  } else {
    result.newcrop = true;
    result.xSide = DraggableRect.RIGHT;
    result.ySide = DraggableRect.BOTTOM;
  }

  return result;
};

/**
 * TODO(JSDOC)
 * @param {number} x X coordinate for cursor.
 * @param {number} y Y coordinate for cursor.
 * @param {boolean} mouseDown  If mouse button is down.
 * @return {string}  // TODO(JSDOC).
 */
DraggableRect.prototype.getCursorStyle = function(x, y, mouseDown) {
  var mode;
  if (mouseDown) {
    mode = this.dragMode_;
  } else {
    mode = this.getDragMode(
        this.viewport_.screenToImageX(x), this.viewport_.screenToImageY(y));
  }
  if (mode.whole) return 'move';
  if (mode.newcrop) return 'crop';
  return this.cssSide_[mode.ySide] + this.cssSide_[mode.xSide] + '-resize';
};

/**
 * TODO(JSDOC)
 * @param {number} x X coordinate for cursor.
 * @param {number} y Y coordinate for cursor.
 * @param {boolean} touch  // TODO(JSDOC).
 * @return {function(number,number)}  // TODO(JSDOC).
 */
DraggableRect.prototype.getDragHandler = function(x, y, touch) {
  x = this.viewport_.screenToImageX(x);
  y = this.viewport_.screenToImageY(y);

  var clipRect = this.viewport_.getImageClipped();
  if (!clipRect.inside(x, y)) return null;

  this.dragMode_ = this.getDragMode(x, y, touch);

  var self = this;

  var mouseBiasX;
  var mouseBiasY;

  var fixedWidth = 0;
  var fixedHeight = 0;

  var resizeFuncX;
  var resizeFuncY;

  if (this.dragMode_.whole) {
    mouseBiasX = this.bounds_.left - x;
    fixedWidth = this.bounds_.right - this.bounds_.left;
    resizeFuncX = function(x) {
      self.bounds_.left = x;
      self.bounds_.right = self.bounds_.left + fixedWidth;
    };
    mouseBiasY = this.bounds_.top - y;
    fixedHeight = this.bounds_.bottom - this.bounds_.top;
    resizeFuncY = function(y) {
      self.bounds_.top = y;
      self.bounds_.bottom = self.bounds_.top + fixedHeight;
    };
  } else {
    var checkNewCrop = function() {
      if (self.dragMode_.newcrop) {
        self.dragMode_.newcrop = false;
        self.bounds_.left = self.bounds_.right = x;
        self.bounds_.top = self.bounds_.bottom = y;
        mouseBiasX = 0;
        mouseBiasY = 0;
      }
    };

    var flipSide = function(side) {
      var opposite = self.oppositeSide_[side];
      var temp = self.bounds_[side];
      self.bounds_[side] = self.bounds_[opposite];
      self.bounds_[opposite] = temp;
      return opposite;
    };

    if (this.dragMode_.xSide != DraggableRect.NONE) {
      mouseBiasX = self.bounds_[this.dragMode_.xSide] - x;
      resizeFuncX = function(x) {
        checkNewCrop();
        self.bounds_[self.dragMode_.xSide] = x;
        if (self.bounds_.left > self.bounds_.right) {
          self.dragMode_.xSide = flipSide(self.dragMode_.xSide);
        }
      };
    }
    if (this.dragMode_.ySide != DraggableRect.NONE) {
      mouseBiasY = self.bounds_[this.dragMode_.ySide] - y;
      resizeFuncY = function(y) {
        checkNewCrop();
        self.bounds_[self.dragMode_.ySide] = y;
        if (self.bounds_.top > self.bounds_.bottom) {
          self.dragMode_.ySide = flipSide(self.dragMode_.ySide);
        }
      };
    }
  }

  function convertX(x) {
    return ImageUtil.clamp(
        clipRect.left,
        self.viewport_.screenToImageX(x) + mouseBiasX,
        clipRect.left + clipRect.width - fixedWidth);
  }

  function convertY(y) {
    return ImageUtil.clamp(
        clipRect.top,
        self.viewport_.screenToImageY(y) + mouseBiasY,
        clipRect.top + clipRect.height - fixedHeight);
  }

  return function(x, y) {
    if (resizeFuncX) resizeFuncX(convertX(x));
    if (resizeFuncY) resizeFuncY(convertY(y));
  };
};

/**
 * TODO(JSDOC)
 * @param {number} x X coordinate for cursor.
 * @param {number} y Y coordinate for cursor.
 * @param {boolean} touch  // TODO(JSDOC).
 * @return {ImageBuffer.DoubleTapAction}  // TODO(JSDOC).
 */
DraggableRect.prototype.getDoubleTapAction = function(x, y, touch) {
  x = this.viewport_.screenToImageX(x);
  y = this.viewport_.screenToImageY(y);

  var clipRect = this.viewport_.getImageClipped();
  if (clipRect.inside(x, y))
    return ImageBuffer.DoubleTapAction.COMMIT;
  else
    return ImageBuffer.DoubleTapAction.NOTHING;
};
