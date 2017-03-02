// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImageEditor is the top level object that holds together and connects
 * everything needed for image editing.
 *
 * @param {!Viewport} viewport The viewport.
 * @param {!ImageView} imageView The ImageView containing the images to edit.
 * @param {!ImageEditorPrompt} prompt Prompt instance.
 * @param {!Object} DOMContainers Various DOM containers required for the
 *     editor.
 * @param {!Array<!ImageEditor.Mode>} modes Available editor modes.
 * @param {function(string, ...string)} displayStringFunction String
 *     formatting function.
 * @constructor
 * @extends {cr.EventTarget}
 * @struct
 *
 * TODO(yawano): Remove displayStringFunction from arguments.
 */
function ImageEditor(
    viewport, imageView, prompt, DOMContainers, modes, displayStringFunction) {
  cr.EventTarget.call(this);

  this.rootContainer_ = DOMContainers.root;
  this.container_ = DOMContainers.image;
  this.modes_ = modes;
  this.displayStringFunction_ = displayStringFunction;

  /**
   * @private {ImageEditor.Mode}
   */
  this.currentMode_ = null;

  /**
   * @private {HTMLElement}
   */
  this.currentTool_ = null;

  /**
   * @private {boolean}
   */
  this.settingUpNextMode_ = false;

  ImageUtil.removeChildren(this.container_);

  this.viewport_ = viewport;

  this.imageView_ = imageView;

  this.buffer_ = new ImageBuffer();
  this.buffer_.addOverlay(this.imageView_);

  this.panControl_ = new ImageEditor.MouseControl(
      this.rootContainer_, this.container_, this.getBuffer());
  this.panControl_.setDoubleTapCallback(this.onDoubleTap_.bind(this));

  this.mainToolbar_ = new ImageEditor.Toolbar(
      DOMContainers.toolbar, displayStringFunction);

  this.modeToolbar_ = new ImageEditor.Toolbar(
      DOMContainers.mode, displayStringFunction,
      this.onOptionsChange.bind(this), true /* done button */);
  this.modeToolbar_.addEventListener(
      'done-clicked', this.onDoneClicked_.bind(this));
  this.modeToolbar_.addEventListener(
      'cancel-clicked', this.onCancelClicked_.bind(this));

  this.prompt_ = prompt;

  this.commandQueue_ = null;

  // -----------------------------------------------------------------
  // Populate the toolbar.

  /**
   * @type {!Array<string>}
   * @private
   */
  this.actionNames_ = [];

  this.mainToolbar_.clear();

  // Create action buttons.
  for (var i = 0; i != this.modes_.length; i++) {
    var mode = this.modes_[i];
    mode.bind(this, this.createToolButton_(mode.name, mode.title,
          this.enterMode.bind(this, mode),
          mode.instant));
  }

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.undoButton_ = this.createToolButton_('undo', 'GALLERY_UNDO',
      this.undo.bind(this),
      true /* instant */);
  this.registerAction_('undo');

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.redoButton_ = this.createToolButton_('redo', 'GALLERY_REDO',
      this.redo.bind(this),
      true /* instant */);
  this.registerAction_('redo');

  /**
   * @private {!HTMLElement}
   * @const
   */
  this.exitButton_ = /** @type {!HTMLElement} */
      (queryRequiredElement('.edit-mode-toolbar paper-button.exit'));
  this.exitButton_.addEventListener('click', this.onExitClicked_.bind(this));

  /**
   * @private {!FilesToast}
   */
  this.filesToast_ = /** @type {!FilesToast}*/
      (queryRequiredElement('files-toast'));
}

ImageEditor.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Handles click event of exit button.
 * @private
 */
ImageEditor.prototype.onExitClicked_ = function() {
  var event = new Event('exit-clicked');
  this.dispatchEvent(event);
};

/**
 * Creates a toolbar button.
 * @param {string} name Button name.
 * @param {string} title Button title.
 * @param {function(Event)} handler onClick handler.
 * @param {boolean} isInstant True if this tool (mode) is instant.
 * @return {!HTMLElement} A created button.
 * @private
 */
ImageEditor.prototype.createToolButton_ = function(
    name, title, handler, isInstant) {
  var button = this.mainToolbar_.addButton(
      title,
      isInstant ? ImageEditor.Toolbar.ButtonType.ICON :
                  ImageEditor.Toolbar.ButtonType.ICON_TOGGLEABLE,
      handler,
      name /* opt_className */);
  return button;
};

/**
 * @return {boolean} True if no user commands are to be accepted.
 */
ImageEditor.prototype.isLocked = function() {
  return !this.commandQueue_ || this.commandQueue_.isBusy();
};

/**
 * @return {boolean} True if the command queue is busy.
 */
ImageEditor.prototype.isBusy = function() {
  return this.commandQueue_ && this.commandQueue_.isBusy();
};

/**
 * Reflect the locked state of the editor in the UI.
 * @param {boolean} on True if locked.
 */
ImageEditor.prototype.lockUI = function(on) {
  ImageUtil.setAttribute(this.rootContainer_, 'locked', on);
};

/**
 * Report the tool use to the metrics subsystem.
 * @param {string} name Action name.
 */
ImageEditor.prototype.recordToolUse = function(name) {
  metrics.recordEnum(
      ImageUtil.getMetricName('Tool'), name, this.actionNames_);
};

/**
 * Content update handler.
 * @private
 */
ImageEditor.prototype.calculateModeApplicativity_ = function() {
  for (var i = 0; i != this.modes_.length; i++) {
    var mode = this.modes_[i];
    ImageUtil.setAttribute(assert(mode.button_), 'disabled',
        !mode.isApplicable());
  }
};

/**
 * Open the editing session for a new image.
 *
 * @param {!GalleryItem} item Gallery item.
 * @param {!ImageView.Effect} effect Transition effect object.
 * @param {function(function())} saveFunction Image save function.
 * @param {function()} displayCallback Display callback.
 * @param {function(!ImageView.LoadType, number, *=)} loadCallback Load
 *     callback.
 */
ImageEditor.prototype.openSession = function(
    item, effect, saveFunction, displayCallback, loadCallback) {
  if (this.commandQueue_)
    throw new Error('Session not closed');

  this.lockUI(true);

  var self = this;
  this.imageView_.load(
      item, effect, displayCallback, function(loadType, delay, error) {
        self.lockUI(false);

        // Always handle an item as original for new session.
        item.setAsOriginal();

        self.commandQueue_ = new CommandQueue(
            self.container_.ownerDocument, assert(self.imageView_.getImage()),
            saveFunction);
        self.commandQueue_.attachUI(
            self.getImageView(), self.getPrompt(), self.filesToast_,
            self.updateUndoRedo.bind(self), self.lockUI.bind(self));
        self.updateUndoRedo();
        loadCallback(loadType, delay, error);
      });
};

/**
 * Close the current image editing session.
 * @param {function()} callback Callback.
 */
ImageEditor.prototype.closeSession = function(callback) {
  this.getPrompt().hide();
  if (this.imageView_.isLoading()) {
    if (this.commandQueue_) {
      console.warn('Inconsistent image editor state');
      this.commandQueue_ = null;
    }
    this.imageView_.cancelLoad();
    this.lockUI(false);
    callback();
    return;
  }
  if (!this.commandQueue_) {
    // Session is already closed.
    callback();
    return;
  }

  this.executeWhenReady(callback);
  this.commandQueue_.close();
  this.commandQueue_ = null;
};

/**
 * Commit the current operation and execute the action.
 *
 * @param {function()} callback Callback.
 */
ImageEditor.prototype.executeWhenReady = function(callback) {
  if (this.commandQueue_) {
    this.leaveMode(false /* not to switch mode */);
    this.commandQueue_.executeWhenReady(callback);
  } else {
    if (!this.imageView_.isLoading())
      console.warn('Inconsistent image editor state');
    callback();
  }
};

/**
 * @return {boolean} True if undo queue is not empty.
 */
ImageEditor.prototype.canUndo = function() {
  return !!this.commandQueue_ && this.commandQueue_.canUndo();
};

/**
 * Undo the recently executed command.
 */
ImageEditor.prototype.undo = function() {
  if (this.isLocked()) return;
  this.recordToolUse('undo');

  // First undo click should dismiss the uncommitted modifications.
  if (this.currentMode_ && this.currentMode_.isUpdated()) {
    this.currentMode_.reset();
    return;
  }

  this.getPrompt().hide();
  this.leaveModeInternal_(false, false /* not to switch mode */);
  this.commandQueue_.undo();
  this.updateUndoRedo();
  this.calculateModeApplicativity_();
};

/**
 * Redo the recently un-done command.
 */
ImageEditor.prototype.redo = function() {
  if (this.isLocked()) return;
  this.recordToolUse('redo');
  this.getPrompt().hide();
  this.leaveModeInternal_(false, false /* not to switch mode */);
  this.commandQueue_.redo();
  this.updateUndoRedo();
  this.calculateModeApplicativity_();
};

/**
 * Update Undo/Redo buttons state.
 */
ImageEditor.prototype.updateUndoRedo = function() {
  var canUndo = this.commandQueue_ && this.commandQueue_.canUndo();
  var canRedo = this.commandQueue_ && this.commandQueue_.canRedo();
  ImageUtil.setAttribute(this.undoButton_, 'disabled', !canUndo);
  ImageUtil.setAttribute(this.redoButton_, 'disabled', !canRedo);
};

/**
 * @return {HTMLCanvasElement|HTMLImageElement} The current image.
 */
ImageEditor.prototype.getImage = function() {
  return this.getImageView().getImage();
};

/**
 * @return {!ImageBuffer} ImageBuffer instance.
 */
ImageEditor.prototype.getBuffer = function() { return this.buffer_; };

/**
 * @return {!ImageView} ImageView instance.
 */
ImageEditor.prototype.getImageView = function() { return this.imageView_; };

/**
 * @return {!Viewport} Viewport instance.
 */
ImageEditor.prototype.getViewport = function() { return this.viewport_; };

/**
 * @return {!ImageEditorPrompt} Prompt instance.
 */
ImageEditor.prototype.getPrompt = function() { return this.prompt_; };

/**
 * Handle the toolbar controls update.
 * @param {Object} options A map of options.
 */
ImageEditor.prototype.onOptionsChange = function(options) {
  ImageUtil.trace.resetTimer('update');
  if (this.currentMode_) {
    this.currentMode_.update(options);
  }
  ImageUtil.trace.reportTimer('update');
};

/**
 * ImageEditor.Mode represents a modal state dedicated to a specific operation.
 * Inherits from ImageBuffer. Overlay to simplify the drawing of mode-specific
 * tools.
 *
 * @param {string} name The mode name.
 * @param {string} title The mode title.
 * @constructor
 * @struct
 * @extends {ImageBuffer.Overlay}
 */
ImageEditor.Mode = function(name, title) {
  this.name = name;
  this.title = title;
  this.message_ = 'GALLERY_ENTER_WHEN_DONE';

  /**
   * @type {boolean}
   */
  this.implicitCommit = false;

  /**
   * @type {boolean}
   */
  this.instant = false;

  /**
   * @type {number}
   */
  this.paddingTop = 0;

  /**
   * @type {number}
   */
  this.paddingBottom = 0;

  /**
   * @type {ImageEditor}
   * @private
   */
  this.editor_ = null;

  /**
   * @type {Viewport}
   * @private
   */
  this.viewport_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.button_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.updated_ = false;

  /**
   * @type {ImageView}
   * @private
   */
  this.imageView_ = null;
};

ImageEditor.Mode.prototype = { __proto__: ImageBuffer.Overlay.prototype };

/**
 * @return {Viewport} Viewport instance.
 */
ImageEditor.Mode.prototype.getViewport = function() { return this.viewport_; };

/**
 * @return {ImageView} ImageView instance.
 */
ImageEditor.Mode.prototype.getImageView = function() {
  return this.imageView_;
};

/**
 * @return {string} The mode-specific message to be displayed when entering.
 */
ImageEditor.Mode.prototype.getMessage = function() { return this.message_; };

/**
 * @return {boolean} True if the mode is applicable in the current context.
 */
ImageEditor.Mode.prototype.isApplicable = function() { return true; };

/**
 * Called once after creating the mode button.
 *
 * @param {!ImageEditor} editor The editor instance.
 * @param {!HTMLElement} button The mode button.
 */

ImageEditor.Mode.prototype.bind = function(editor, button) {
  this.editor_ = editor;
  this.editor_.registerAction_(this.name);
  this.button_ = button;
  this.viewport_ = editor.getViewport();
  this.imageView_ = editor.getImageView();
};

/**
 * Called before entering the mode.
 */
ImageEditor.Mode.prototype.setUp = function() {
  this.editor_.getBuffer().addOverlay(this);
  this.updated_ = false;
};

/**
 * Create mode-specific controls here.
 * @param {!ImageEditor.Toolbar} toolbar The toolbar to populate.
 */
ImageEditor.Mode.prototype.createTools = function(toolbar) {};

/**
 * Called before exiting the mode.
 */
ImageEditor.Mode.prototype.cleanUpUI = function() {
  this.editor_.getBuffer().removeOverlay(this);
};

/**
 * Called after exiting the mode.
 */
ImageEditor.Mode.prototype.cleanUpCaches = function() {};

/**
 * Called when any of the controls changed its value.
 * @param {Object} options A map of options.
 */
ImageEditor.Mode.prototype.update = function(options) {
  this.markUpdated();
};

/**
 * Mark the editor mode as updated.
 */
ImageEditor.Mode.prototype.markUpdated = function() {
  this.updated_ = true;
};

/**
 * @return {boolean} True if the mode controls changed.
 */
ImageEditor.Mode.prototype.isUpdated = function() { return this.updated_; };

/**
 * @return {boolean} True if a key event should be consumed by the mode.
 */
ImageEditor.Mode.prototype.isConsumingKeyEvents = function() { return false; };

/**
 * Resets the mode to a clean state.
 */
ImageEditor.Mode.prototype.reset = function() {
  this.editor_.modeToolbar_.reset();
  this.updated_ = false;
};

/**
 * @return {Command} Command.
 */
ImageEditor.Mode.prototype.getCommand = function() {
  return null;
};

/**
 * One-click editor tool, requires no interaction, just executes the command.
 *
 * @param {string} name The mode name.
 * @param {string} title The mode title.
 * @param {!Command} command The command to execute on click.
 * @constructor
 * @extends {ImageEditor.Mode}
 * @struct
 */
ImageEditor.Mode.OneClick = function(name, title, command) {
  ImageEditor.Mode.call(this, name, title);
  this.instant = true;
  this.command_ = command;
};

ImageEditor.Mode.OneClick.prototype = {__proto__: ImageEditor.Mode.prototype};

/**
 * @override
 */
ImageEditor.Mode.OneClick.prototype.getCommand = function() {
  return this.command_;
};

/**
 * Register the action name. Required for metrics reporting.
 * @param {string} name Button name.
 * @private
 */
ImageEditor.prototype.registerAction_ = function(name) {
  this.actionNames_.push(name);
};

/**
 * @return {ImageEditor.Mode} The current mode.
 */
ImageEditor.prototype.getMode = function() { return this.currentMode_; };

/**
 * The user clicked on the mode button.
 *
 * @param {!ImageEditor.Mode} mode The new mode.
 */
ImageEditor.prototype.enterMode = function(mode) {
  if (this.isLocked()) return;

  if (this.currentMode_ === mode) {
    // Currently active editor tool clicked, commit if modified.
    this.leaveModeInternal_(
        this.currentMode_.updated_, false /* not to switch mode */);
    return;
  }

  // Guard not to call setUpMode_ more than once.
  if (this.settingUpNextMode_)
    return;
  this.settingUpNextMode_ = true;

  this.recordToolUse(mode.name);

  this.leaveMode(true /* to switch mode */);

  // The above call could have caused a commit which might have initiated
  // an asynchronous command execution. Wait for it to complete, then proceed
  // with the mode set up.
  this.commandQueue_.executeWhenReady(function() {
    this.setUpMode_(mode);
    this.settingUpNextMode_ = false;
  }.bind(this));
};

/**
 * Set up the new editing mode.
 *
 * @param {!ImageEditor.Mode} mode The mode.
 * @private
 */
ImageEditor.prototype.setUpMode_ = function(mode) {
  this.currentTool_ = mode.button_;
  this.currentMode_ = mode;
  this.rootContainer_.setAttribute('editor-mode', mode.name);

  // Activate toggle ripple if button is toggleable.
  var filesToggleRipple =
      this.currentTool_.querySelector('files-toggle-ripple');
  if (filesToggleRipple) {
    // Current mode must NOT be instant for toggleable button.
    assert(!this.currentMode_.instant);
    filesToggleRipple.activated = true;
  }

  // Scale the screen so that it doesn't overlap the toolbars. We should scale
  // the screen before setup of current mode is called to make the current mode
  // able to set up with new screen size.
  if (!this.currentMode_.instant) {
    this.getViewport().setScreenTop(
        ImageEditor.Toolbar.HEIGHT + mode.paddingTop);
    this.getViewport().setScreenBottom(
        ImageEditor.Toolbar.HEIGHT * 2 + mode.paddingBottom);
    this.getImageView().applyViewportChange();
  }

  this.currentMode_.setUp();

  this.calculateModeApplicativity_();
  if (this.currentMode_.instant) {  // Instant tool.
    this.leaveModeInternal_(true, false /* not to switch mode */);
    return;
  }

  this.exitButton_.hidden = true;

  this.modeToolbar_.clear();
  this.currentMode_.createTools(this.modeToolbar_);
  this.modeToolbar_.show(true);
};

/**
 * Handles click event of Done button.
 * @param {!Event} event An event.
 * @private
 */
ImageEditor.prototype.onDoneClicked_ = function(event) {
  this.leaveModeInternal_(true /* commit */, false /* not to switch mode */);
};

/**
 * Handles click event of Cancel button.
 * @param {!Event} event An event.
 * @private
 */
ImageEditor.prototype.onCancelClicked_ = function(event) {
  this.leaveModeInternal_(
      false /* not commit */, false /* not to switch mode */);
};

/**
 * The user clicked on 'OK' or 'Cancel' or on a different mode button.
 * @param {boolean} commit True if commit is required.
 * @param {boolean} leaveToSwitchMode True if it leaves to change mode.
 * @private
 */
ImageEditor.prototype.leaveModeInternal_ = function(commit, leaveToSwitchMode) {
  if (!this.currentMode_)
    return;

  // If the current mode is 'Resize', and commit is required,
  // leaving mode should be stopped when an input value is not valid.
  if(commit && this.currentMode_.name === 'resize') {
    var resizeMode = /** @type {!ImageEditor.Mode.Resize} */
                              (this.currentMode_);
    if(!resizeMode.isInputValid()) {
      resizeMode.showAlertDialog();
      return;
    }
  }

  this.modeToolbar_.show(false);
  this.rootContainer_.removeAttribute('editor-mode');

  // If it leaves to switch mode, do not restore screen size since the next mode
  // might change screen size. We should avoid to show intermediate animation
  // which tries to restore screen size.
  if (!leaveToSwitchMode) {
    this.getViewport().setScreenTop(ImageEditor.Toolbar.HEIGHT);
    this.getViewport().setScreenBottom(ImageEditor.Toolbar.HEIGHT);
    this.getImageView().applyViewportChange();
  }

  this.currentMode_.cleanUpUI();

  if (commit) {
    var self = this;
    var command = this.currentMode_.getCommand();
    if (command) {  // Could be null if the user did not do anything.
      this.commandQueue_.execute(command);
      this.updateUndoRedo();
    }
  }

  var filesToggleRipple =
      this.currentTool_.querySelector('files-toggle-ripple');
  if (filesToggleRipple)
    filesToggleRipple.activated = false;

  this.exitButton_.hidden = false;

  this.currentMode_.cleanUpCaches();
  this.currentMode_ = null;
  this.currentTool_ = null;
};

/**
 * Leave the mode, commit only if required by the current mode.
 * @param {boolean} leaveToSwitchMode True if it leaves to switch mode.
 */
ImageEditor.prototype.leaveMode = function(leaveToSwitchMode) {
  this.leaveModeInternal_(!!this.currentMode_ &&
      this.currentMode_.updated_ &&
      this.currentMode_.implicitCommit,
      leaveToSwitchMode);
};

/**
 * Enter the editor mode with the given name.
 *
 * @param {string} name Mode name.
 * @private
 */
ImageEditor.prototype.enterModeByName_ = function(name) {
  for (var i = 0; i !== this.modes_.length; i++) {
    var mode = this.modes_[i];
    if (mode.name === name) {
      if (!mode.button_.hasAttribute('disabled'))
        this.enterMode(mode);
      return;
    }
  }
  console.error('Mode "' + name + '" not found.');
};

/**
 * Key down handler.
 * @param {!Event} event The keydown event.
 * @return {boolean} True if handled.
 */
ImageEditor.prototype.onKeyDown = function(event) {
  if (this.currentMode_ && this.currentMode_.isConsumingKeyEvents())
    return false;

  switch (util.getKeyModifiers(event) + event.key) {
    case 'Escape':
    case 'Enter':
      if (this.getMode()) {
        this.leaveModeInternal_(event.key === 'Enter',
            false /* not to switch mode */);
        return true;
      }
      break;

    case 'Ctrl-z':  // Ctrl+Z
      if (this.commandQueue_.canUndo()) {
        this.undo();
        return true;
      }
      break;

    case 'Ctrl-y':  // Ctrl+Y
      if (this.commandQueue_.canRedo()) {
        this.redo();
        return true;
      }
      break;

    case 'a':
      this.enterModeByName_('autofix');
      return true;

    case 'b':
      this.enterModeByName_('exposure');
      return true;

    case 'c':
      this.enterModeByName_('crop');
      return true;

    case 'l':
      this.enterModeByName_('rotate_left');
      return true;

    case 'r':
      this.enterModeByName_('rotate_right');
      return true;
  }
  return false;
};

/**
 * Double tap handler.
 * @param {number} x X coordinate of the event.
 * @param {number} y Y coordinate of the event.
 * @private
 */
ImageEditor.prototype.onDoubleTap_ = function(x, y) {
  if (this.getMode()) {
    var action = this.buffer_.getDoubleTapAction(x, y);
    if (action === ImageBuffer.DoubleTapAction.COMMIT)
      this.leaveModeInternal_(true, false /* not to switch mode */);
    else if (action === ImageBuffer.DoubleTapAction.CANCEL)
      this.leaveModeInternal_(false, false /* not to switch mode */);
  }
};

/**
 * Called when the user starts editing image.
 */
ImageEditor.prototype.onStartEditing = function() {
  this.calculateModeApplicativity_();
};

/**
 * A helper object for panning the ImageBuffer.
 *
 * @param {!HTMLElement} rootContainer The top-level container.
 * @param {!HTMLElement} container The container for mouse events.
 * @param {!ImageBuffer} buffer Image buffer.
 * @constructor
 * @struct
 */
ImageEditor.MouseControl = function(rootContainer, container, buffer) {
  this.rootContainer_ = rootContainer;
  this.container_ = container;
  this.buffer_ = buffer;

  var handlers = {
    'touchstart': this.onTouchStart,
    'touchend': this.onTouchEnd,
    'touchcancel': this.onTouchCancel,
    'touchmove': this.onTouchMove,
    'mousedown': this.onMouseDown,
    'mouseup': this.onMouseUp
  };

  for (var eventName in handlers) {
    container.addEventListener(
        eventName, handlers[eventName].bind(this), false);
  }

  // Mouse move handler has to be attached to the window to receive events
  // from outside of the window. See: http://crbug.com/155705
  window.addEventListener('mousemove', this.onMouseMove.bind(this), false);

  /**
   * @type {?ImageBuffer.DragHandler}
   * @private
   */
  this.dragHandler_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.dragHappened_ = false;

  /**
   * @type {?{x: number, y: number, time:number}}
   * @private
   */
  this.touchStartInfo_ = null;

  /**
   * @type {?{x: number, y: number, time:number}}
   * @private
   */
  this.previousTouchStartInfo_ = null;
};

/**
 * Maximum movement for touch to be detected as a tap (in pixels).
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_MOVEMENT_FOR_TAP_ = 8;

/**
 * Maximum time for touch to be detected as a tap (in milliseconds).
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_TAP_DURATION_ = 500;

/**
 * Maximum distance from the first tap to the second tap to be considered
 * as a double tap.
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_DISTANCE_FOR_DOUBLE_TAP_ = 32;

/**
 * Maximum time for touch to be detected as a double tap (in milliseconds).
 * @private
 * @const
 */
ImageEditor.MouseControl.MAX_DOUBLE_TAP_DURATION_ = 1000;

/**
 * Returns an event's position.
 *
 * @param {!(MouseEvent|Touch)} e Pointer position.
 * @return {!Object} A pair of x,y in page coordinates.
 * @private
 */
ImageEditor.MouseControl.getPosition_ = function(e) {
  return {
    x: e.pageX,
    y: e.pageY
  };
};

/**
 * Returns touch position or null if there is more than one touch position.
 *
 * @param {!TouchEvent} e Event.
 * @return {Object?} A pair of x,y in page coordinates.
 * @private
 */
ImageEditor.MouseControl.prototype.getTouchPosition_ = function(e) {
  if (e.targetTouches.length == 1)
    return ImageEditor.MouseControl.getPosition_(e.targetTouches[0]);
  else
    return null;
};

/**
 * Touch start handler.
 * @param {!TouchEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onTouchStart = function(e) {
  var position = this.getTouchPosition_(e);
  if (position) {
    this.touchStartInfo_ = {
      x: position.x,
      y: position.y,
      time: Date.now()
    };
    this.dragHandler_ = this.buffer_.getDragHandler(position.x, position.y,
                                                    true /* touch */);
    this.dragHappened_ = false;
  }
};

/**
 * Touch end handler.
 * @param {!TouchEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onTouchEnd = function(e) {
  if (!this.dragHappened_ &&
      this.touchStartInfo_ &&
      Date.now() - this.touchStartInfo_.time <=
          ImageEditor.MouseControl.MAX_TAP_DURATION_) {
    this.buffer_.onClick(this.touchStartInfo_.x, this.touchStartInfo_.y);
    if (this.previousTouchStartInfo_ &&
        Date.now() - this.previousTouchStartInfo_.time <
            ImageEditor.MouseControl.MAX_DOUBLE_TAP_DURATION_) {
      var prevTouchCircle = new Circle(
          this.previousTouchStartInfo_.x,
          this.previousTouchStartInfo_.y,
          ImageEditor.MouseControl.MAX_DISTANCE_FOR_DOUBLE_TAP_);
      if (prevTouchCircle.inside(this.touchStartInfo_.x,
                                 this.touchStartInfo_.y)) {
        this.doubleTapCallback_(this.touchStartInfo_.x, this.touchStartInfo_.y);
      }
    }
    this.previousTouchStartInfo_ = this.touchStartInfo_;
  } else {
    this.previousTouchStartInfo_ = null;
  }
  this.onTouchCancel();
};

/**
 * Default double tap handler.
 * @param {number} x X coordinate of the event.
 * @param {number} y Y coordinate of the event.
 * @private
 */
ImageEditor.MouseControl.prototype.doubleTapCallback_ = function(x, y) {};

/**
 * Sets callback to be called when double tap detected.
 * @param {function(number, number)} callback New double tap callback.
 */
ImageEditor.MouseControl.prototype.setDoubleTapCallback = function(callback) {
  this.doubleTapCallback_ = callback;
};

/**
 * Touch cancel handler.
 */
ImageEditor.MouseControl.prototype.onTouchCancel = function() {
  this.dragHandler_ = null;
  this.dragHappened_ = false;
  this.touchStartInfo_ = null;
  this.lockMouse_(false);
};

/**
 * Touch move handler.
 * @param {!TouchEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onTouchMove = function(e) {
  var position = this.getTouchPosition_(e);
  if (!position)
    return;

  if (this.touchStartInfo_ && !this.dragHappened_) {
    var tapCircle = new Circle(
        this.touchStartInfo_.x, this.touchStartInfo_.y,
        ImageEditor.MouseControl.MAX_MOVEMENT_FOR_TAP_);
    this.dragHappened_ = !tapCircle.inside(position.x, position.y);
  }
  if (this.dragHandler_ && this.dragHappened_) {
    this.dragHandler_(position.x, position.y, e.shiftKey);
    this.lockMouse_(true);
  }
};

/**
 * Mouse down handler.
 * @param {!MouseEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onMouseDown = function(e) {
  var position = ImageEditor.MouseControl.getPosition_(e);

  this.dragHandler_ = this.buffer_.getDragHandler(position.x, position.y,
                                                  false /* mouse */);
  this.dragHappened_ = false;
  this.updateCursor_(position);
};

/**
 * Mouse up handler.
 * @param {!MouseEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onMouseUp = function(e) {
  var position = ImageEditor.MouseControl.getPosition_(e);

  if (!this.dragHappened_) {
    this.buffer_.onClick(position.x, position.y);
  }
  this.dragHandler_ = null;
  this.dragHappened_ = false;
  this.lockMouse_(false);
};

/**
 * Mouse move handler.
 * @param {!Event} e Event.
 */
ImageEditor.MouseControl.prototype.onMouseMove = function(e) {
  e = assertInstanceof(e, MouseEvent);
  var position = ImageEditor.MouseControl.getPosition_(e);

  if (this.dragHandler_ && !e.which) {
    // mouseup must have happened while the mouse was outside our window.
    this.dragHandler_ = null;
    this.lockMouse_(false);
  }

  this.updateCursor_(position);
  if (this.dragHandler_) {
    this.dragHandler_(position.x, position.y, e.shiftKey);
    this.dragHappened_ = true;
    this.lockMouse_(true);
  }
};

/**
 * Update the UI to reflect mouse drag state.
 * @param {boolean} on True if dragging.
 * @private
 */
ImageEditor.MouseControl.prototype.lockMouse_ = function(on) {
  ImageUtil.setAttribute(this.rootContainer_, 'mousedrag', on);
};

/**
 * Update the cursor.
 *
 * @param {!Object} position An object holding x and y properties.
 * @private
 */
ImageEditor.MouseControl.prototype.updateCursor_ = function(position) {
  var oldCursor = this.container_.getAttribute('cursor');
  var newCursor = this.buffer_.getCursorStyle(
      position.x, position.y, !!this.dragHandler_);
  if (newCursor != oldCursor)  // Avoid flicker.
    this.container_.setAttribute('cursor', newCursor);
};

/**
 * A toolbar for the ImageEditor.
 * @param {!HTMLElement} parent The parent element.
 * @param {function(string)} displayStringFunction A string formatting function.
 * @param {function(Object)=} opt_updateCallback The callback called when
 *     controls change.
 * @param {boolean=} opt_showActionButtons True to show action buttons.
 * @constructor
 * @extends {cr.EventTarget}
 * @struct
 */
ImageEditor.Toolbar = function(
    parent, displayStringFunction, opt_updateCallback, opt_showActionButtons) {
  this.wrapper_ = parent;
  this.displayStringFunction_ = displayStringFunction;

  /**
   * @type {?function(Object)}
   * @private
   */
  this.updateCallback_ = opt_updateCallback || null;

  /**
   * @private {!HTMLElement}
   */
  this.container_ = /** @type {!HTMLElement} */ (document.createElement('div'));
  this.container_.classList.add('container');
  this.wrapper_.appendChild(this.container_);

    // Create action buttons.
  if (opt_showActionButtons) {
    var actionButtonsLayer = document.createElement('div');
    actionButtonsLayer.classList.add('action-buttons');

    this.cancelButton_ = ImageEditor.Toolbar.createButton_(
        'GALLERY_CANCEL_LABEL', ImageEditor.Toolbar.ButtonType.LABEL_UPPER_CASE,
        this.onCancelClicked_.bind(this), 'cancel');
    actionButtonsLayer.appendChild(this.cancelButton_);

    this.doneButton_ = ImageEditor.Toolbar.createButton_(
        'GALLERY_DONE', ImageEditor.Toolbar.ButtonType.LABEL_UPPER_CASE,
        this.onDoneClicked_.bind(this), 'done');
    actionButtonsLayer.appendChild(this.doneButton_);

    this.wrapper_.appendChild(actionButtonsLayer);
  }
};

ImageEditor.Toolbar.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Height of the toolbar.
 * @const {number}
 */
ImageEditor.Toolbar.HEIGHT = 48; // px

/**
 * Handles click event of done button.
 * @private
 */
ImageEditor.Toolbar.prototype.onDoneClicked_ = function() {
  this.doneButton_.querySelector('paper-ripple').simulatedRipple();

  var event = new Event('done-clicked');
  this.dispatchEvent(event);
};

/**
 * Handles click event of cancel button.
 * @private
 */
ImageEditor.Toolbar.prototype.onCancelClicked_ = function() {
  this.cancelButton_.querySelector('paper-ripple').simulatedRipple();

  var event = new Event('cancel-clicked');
  this.dispatchEvent(event);
};

/**
 * Returns the parent element.
 * @return {!HTMLElement}
 */
ImageEditor.Toolbar.prototype.getElement = function() {
  return this.container_;
};

/**
 * Clear the toolbar.
 */
ImageEditor.Toolbar.prototype.clear = function() {
  ImageUtil.removeChildren(this.container_);
};

/**
 * Add a control.
 * @param {!HTMLElement} element The control to add.
 * @return {!HTMLElement} The added element.
 */
ImageEditor.Toolbar.prototype.add = function(element) {
  this.container_.appendChild(element);
  return element;
};

/**
 * Button type.
 * @enum {string}
 */
ImageEditor.Toolbar.ButtonType = {
  ICON: 'icon',
  ICON_TOGGLEABLE: 'icon_toggleable',
  LABEL: 'label',
  LABEL_UPPER_CASE: 'label_upper_case'
};

/**
 * Create a button.
 *
 * @param {string} title String ID of button title.
 * @param {ImageEditor.Toolbar.ButtonType} type Button type.
 * @param {function(Event)} handler onClick handler.
 * @param {string=} opt_class Extra class name.
 * @return {!HTMLElement} The created button.
 * @private
 */
ImageEditor.Toolbar.createButton_ = function(
    title, type, handler, opt_class) {
  var button = /** @type {!HTMLElement} */ (document.createElement('button'));
  if (opt_class)
    button.classList.add(opt_class);
  button.classList.add('edit-toolbar');

  if (type === ImageEditor.Toolbar.ButtonType.ICON ||
      type === ImageEditor.Toolbar.ButtonType.ICON_TOGGLEABLE) {
    var icon = document.createElement('div');
    icon.classList.add('icon');

    // Show tooltip for icon button.
    assertInstanceof(document.querySelector('files-tooltip'), FilesTooltip)
        .addTarget(button);

    button.appendChild(icon);

    if (type === ImageEditor.Toolbar.ButtonType.ICON) {
      var filesRipple = document.createElement('files-ripple');
      button.appendChild(filesRipple);
    } else {
      var filesToggleRipple = document.createElement('files-toggle-ripple');
      button.appendChild(filesToggleRipple);
    }
  } else if (type === ImageEditor.Toolbar.ButtonType.LABEL ||
      type === ImageEditor.Toolbar.ButtonType.LABEL_UPPER_CASE) {
    var label = document.createElement('span');
    label.classList.add('label');
    label.textContent =
        type === ImageEditor.Toolbar.ButtonType.LABEL_UPPER_CASE ?
        strf(title).toLocaleUpperCase() : strf(title);

    button.appendChild(label);

    var paperRipple = document.createElement('paper-ripple');
    button.appendChild(paperRipple);
  } else {
    assertNotReached();
  }

  button.label = strf(title);
  button.setAttribute('aria-label', strf(title));

  GalleryUtil.decorateMouseFocusHandling(button);

  button.addEventListener('click', handler, false);
  button.addEventListener('keydown', function(event) {
    // Stop propagation of Enter key event to prevent it from being captured by
    // image editor.
    if (event.key === 'Enter')
      event.stopPropagation();
  });

  return button;
};

/**
 * Add a button.
 *
 * @param {string} title Button title.
 * @param {ImageEditor.Toolbar.ButtonType} type Button type.
 * @param {function(Event)} handler onClick handler.
 * @param {string=} opt_class Extra class name.
 * @return {!HTMLElement} The added button.
 */
ImageEditor.Toolbar.prototype.addButton = function(
    title, type, handler, opt_class) {
  var button = ImageEditor.Toolbar.createButton_(
      title, type, handler, opt_class);
  this.add(button);
  return button;
};

/**
 * Add a input field.
 *
 * @param {string} name Input name
 * @param {string} title Input title
 * @param {function(Event)} handler onInput and onChange handler
 * @param {string|number} value Default value
 * @param {string=} opt_unit Unit for an input field
 * @return {!HTMLElement} Input Element
 */
ImageEditor.Toolbar.prototype.addInput = function(
    name, title, handler, value, opt_unit) {

  var input = /** @type {!HTMLElement} */ (document.createElement('div'));
  input.classList.add('input', name);

  var text = document.createElement('paper-input');
  text.setAttribute('label', strf(title));
  text.classList.add('text', name);
  text.value = value;

  // We should listen to not only 'change' event, but also 'input' because we
  // want to update values as soon as the user types characters.
  text.addEventListener('input', handler, false);
  text.addEventListener('change', handler, false);
  input.appendChild(text);

  if(opt_unit) {
    var unit_label = document.createElement('span');
    unit_label.textContent = opt_unit;
    unit_label.classList.add('unit_label');
    input.appendChild(unit_label);
  }

  input.name = name;
  input.getValue = function(text) {
    return text.value;
  }.bind(this, text);
  input.setValue = function(text, value) {
    text.value = value;
  }.bind(this, text);

  this.add(input);

  return input;
};

/**
 * Add a range control (scalar value picker).
 *
 * @param {string} name An option name.
 * @param {string} title An option title.
 * @param {number} min Min value of the option.
 * @param {number} value Default value of the option.
 * @param {number} max Max value of the options.
 * @param {number=} opt_scale A number to multiply by when setting
 *     min/value/max in DOM.
 * @param {boolean=} opt_showNumeric True if numeric value should be displayed.
 * @return {!HTMLElement} Range element.
 */
ImageEditor.Toolbar.prototype.addRange = function(
    name, title, min, value, max, opt_scale, opt_showNumeric) {
  var range = /** @type {!HTMLElement} */ (document.createElement('div'));
  range.classList.add('range', name);

  var icon = document.createElement('icon');
  icon.classList.add('icon');
  range.appendChild(icon);

  var label = document.createElement('span');
  label.textContent = strf(title);
  label.classList.add('label');
  range.appendChild(label);

  var scale = opt_scale || 1;
  var slider = document.createElement('paper-slider');
  slider.min = Math.ceil(min * scale);
  slider.max = Math.floor(max * scale);
  slider.value = value * scale;
  slider.addEventListener('change', function(event) {
    if (this.updateCallback_)
      this.updateCallback_(this.getOptions());
  }.bind(this));
  range.appendChild(slider);

  range.name = name;
  range.getValue = function(slider, scale) {
    return slider.value / scale;
  }.bind(this, slider, scale);

  // Swallow the left and right keys, so they are not handled by other
  // listeners.
  range.addEventListener('keydown', function(e) {
    if (e.key === 'ArrowLeft' || e.key === 'ArrowRight')
      e.stopPropagation();
  });

  this.add(range);

  return range;
};

/**
 * @return {!Object} options A map of options.
 */
ImageEditor.Toolbar.prototype.getOptions = function() {
  var values = {};

  for (var child = this.container_.firstChild;
       child;
       child = child.nextSibling) {
    if (child.name)
      values[child.name] = child.getValue();
  }

  return values;
};

/**
 * Reset the toolbar.
 */
ImageEditor.Toolbar.prototype.reset = function() {
  for (var child = this.wrapper_.firstChild; child; child = child.nextSibling) {
    if (child.reset) child.reset();
  }
};

/**
 * Show/hide the toolbar.
 * @param {boolean} on True if show.
 */
ImageEditor.Toolbar.prototype.show = function(on) {
  if (!this.wrapper_.firstChild)
    return;  // Do not show empty toolbar;

  this.wrapper_.hidden = !on;

  // Focus the first input on the toolbar.
  if (on) {
    var input = this.container_.querySelector(
       'button, paper-button, input, paper-input, paper-slider');
    if (input)
      input.focus();
  }
};
