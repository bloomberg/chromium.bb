// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The root of the file manager's view managing the DOM of Files.app.
 *
 * @param {!HTMLElement} element Top level element of Files.app.
 * @param {!LaunchParam} launchParam Launch param.
 * @constructor
 * @struct
 */
function FileManagerUI(element, launchParam) {
  // Pre-populate the static localized strings.
  i18nTemplate.process(element.ownerDocument, loadTimeData);

  // Initialize the dialog label. This should be done before constructing dialog
  // instances.
  cr.ui.dialogs.BaseDialog.OK_LABEL = str('OK_LABEL');
  cr.ui.dialogs.BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');

  /**
   * Top level element of Files.app.
   * @type {!HTMLElement}
   */
  this.element = element;

  /**
   * Dialog type.
   * @type {DialogType}
   * @private
   */
  this.dialogType_ = launchParam.type;

  /**
   * Error dialog.
   * @type {!ErrorDialog}
   * @const
   */
  this.errorDialog = new ErrorDialog(this.element);

  /**
   * Alert dialog.
   * @type {!cr.ui.dialogs.AlertDialog}
   * @const
   */
  this.alertDialog = new cr.ui.dialogs.AlertDialog(this.element);

  /**
   * Confirm dialog.
   * @type {!cr.ui.dialogs.ConfirmDialog}
   * @const
   */
  this.confirmDialog = new cr.ui.dialogs.ConfirmDialog(this.element);

  /**
   * Confirm dialog for delete.
   * @type {!cr.ui.dialogs.ConfirmDialog}
   * @const
   */
  this.deleteConfirmDialog = new cr.ui.dialogs.ConfirmDialog(this.element);
  this.deleteConfirmDialog.setOkLabel(str('DELETE_BUTTON_LABEL'));

  /**
   * Prompt dialog.
   * @type {!cr.ui.dialogs.PromptDialog}
   * @const
   */
  this.promptDialog = new cr.ui.dialogs.PromptDialog(this.element);

  /**
   * Share dialog.
   * @type {!ShareDialog}
   * @const
   */
  this.shareDialog = new ShareDialog(this.element);

  /**
   * Multi-profile share dialog.
   * @type {!MultiProfileShareDialog}
   * @const
   */
  this.multiProfileShareDialog = new MultiProfileShareDialog(this.element);

  /**
   * Default task picker.
   * @type {!cr.filebrowser.DefaultActionDialog}
   * @const
   */
  this.defaultTaskPicker =
      new cr.filebrowser.DefaultActionDialog(this.element);

  /**
   * Suggest apps dialog.
   * @type {!SuggestAppsDialog}
   * @const
   */
  this.suggestAppsDialog = new SuggestAppsDialog(
      this.element, launchParam.suggestAppsDialogState);

  /**
   * Conflict dialog.
   * @type {!ConflictDialog}
   * @const
   */
  this.conflictDialog = new ConflictDialog(this.element);

  /**
   * The container element of the dialog.
   * @type {!HTMLElement}
   * @private
   */
  this.dialogContainer =
      queryRequiredElement(this.element, '.dialog-container');

  /**
   * Context menu for texts.
   * @type {!cr.ui.Menu}
   * @const
   */
  this.textContextMenu = util.queryDecoratedElement(
      '#text-context-menu', cr.ui.Menu);

  /**
   * Location line.
   * @type {LocationLine}
   */
  this.locationLine = null;

  /**
   * The toolbar which contains controls.
   * @type {!HTMLElement}
   * @const
   */
  this.toolbar = queryRequiredElement(this.element, '.dialog-header');

  /**
   * The navigation list.
   * @type {!HTMLElement}
   * @const
   */
  this.dialogNavigationList =
      queryRequiredElement(this.element, '.dialog-navigation-list');

  /**
   * Search box.
   * @type {!SearchBox}
   * @const
   */
  this.searchBox = new SearchBox(
      this.element.querySelector('#search-box'),
      this.element.querySelector('#search-button'),
      this.element.querySelector('#no-search-results'));

  /**
   * Toggle-view button.
   * @type {!Element}
   * @const
   */
  this.toggleViewButton = queryRequiredElement(this.element, '#view-button');

  /**
   * The button to open gear menu.
   * @type {!cr.ui.MenuButton}
   * @const
   */
  this.gearButton = util.queryDecoratedElement(
      '#gear-button', cr.ui.MenuButton);

  /**
   * @type {!GearMenu}
   * @const
   */
  this.gearMenu = new GearMenu(this.gearButton.menu);

  /**
   * Directory tree.
   * @type {DirectoryTree}
   */
  this.directoryTree = null;

  /**
   * Progress center panel.
   * @type {!ProgressCenterPanel}
   * @const
   */
  this.progressCenterPanel = new ProgressCenterPanel(
      queryRequiredElement(this.element, '#progress-center'));

  /**
   * List container.
   * @type {ListContainer}
   */
  this.listContainer = null;

  /**
   * @type {!HTMLElement}
   */
  this.formatPanelError =
      queryRequiredElement(this.element, '#format-panel > .error');

  /**
   * @type {!cr.ui.Menu}
   * @const
   */
  this.fileContextMenu = util.queryDecoratedElement(
      '#file-context-menu', cr.ui.Menu);

  /**
   * @type {!HTMLMenuItemElement}
   * @const
   */
  this.fileContextMenu.defaultActionMenuItem =
      /** @type {!HTMLMenuItemElement} */
      (queryRequiredElement(this.fileContextMenu, '#default-action'));

  /**
   * @type {!HTMLElement}
   * @const
   */
  this.fileContextMenu.defaultActionSeparator =
      queryRequiredElement(this.fileContextMenu, '#default-action-separator');

  /**
   * The combo button to specify the task.
   * @type {!cr.ui.ComboButton}
   * @const
   */
  this.taskMenuButton = util.queryDecoratedElement(
      '#tasks', cr.ui.ComboButton);
  this.taskMenuButton.showMenu = function(shouldSetFocus) {
    // Prevent the empty menu from opening.
    if (!this.menu.length)
      return;
    cr.ui.ComboButton.prototype.showMenu.call(this, shouldSetFocus);
  };

  /**
   * Banners in the file list.
   * @type {Banners}
   */
  this.banners = null;

  /**
   * Dialog footer.
   * @type {!DialogFooter}
   */
  this.dialogFooter = DialogFooter.findDialogFooter(
      this.dialogType_, /** @type {!Document} */(this.element.ownerDocument));

  // Initialize attributes.
  this.element.setAttribute('type', this.dialogType_);

  // Modify UI default behavior.
  this.element.addEventListener('click', this.onExternalLinkClick_.bind(this));
  this.element.addEventListener('drop', function(e) {
    e.preventDefault();
  });
  if (util.runningInBrowser()) {
    this.element.addEventListener('contextmenu', function(e) {
      e.preventDefault();
      e.stopPropagation();
    });
  }
}

/**
 * Initializes here elements, which are expensive or hidden in the beginning.
 *
 * @param {!FileTable} table
 * @param {!FileGrid} grid
 * @param {!LocationLine} locationLine
 */
FileManagerUI.prototype.initAdditionalUI = function(
    table, grid, locationLine) {
  // List container.
  this.listContainer = new ListContainer(
      queryRequiredElement(this.element, '#list-container'), table, grid);

  // Splitter.
  this.decorateSplitter_(
      queryRequiredElement(this.element, '#navigation-list-splitter'));

  // Location line.
  this.locationLine = locationLine;

  // Init context menus.
  cr.ui.contextMenuHandler.setContextMenu(grid, this.fileContextMenu);
  cr.ui.contextMenuHandler.setContextMenu(table.list, this.fileContextMenu);
  cr.ui.contextMenuHandler.setContextMenu(
      queryRequiredElement(document, '.drive-welcome.page'),
      this.fileContextMenu);

  // Add handlers.
  document.defaultView.addEventListener('resize', this.relayout.bind(this));
  document.addEventListener('focusout', this.onFocusOut_.bind(this));

  // Set the initial focus.
  this.onFocusOut_();
};

/**
 * TODO(hirono): Merge the method into initAdditionalUI.
 * @param {!DirectoryTree} directoryTree
 */
FileManagerUI.prototype.initDirectoryTree = function(directoryTree) {
  this.directoryTree = directoryTree;

  // Set up the context menu for the volume/shortcut items in directory tree.
  this.directoryTree.contextMenuForRootItems =
      util.queryDecoratedElement('#roots-context-menu', cr.ui.Menu);
  this.directoryTree.contextMenuForSubitems =
      util.queryDecoratedElement('#directory-tree-context-menu', cr.ui.Menu);

  // Visible height of the directory tree depends on the size of progress
  // center panel. When the size of progress center panel changes, directory
  // tree has to be notified to adjust its components (e.g. progress bar).
  var observer =
      new MutationObserver(directoryTree.relayout.bind(directoryTree));
  observer.observe(this.progressCenterPanel.element,
                   /** @type {MutationObserverInit} */
                   ({subtree: true, attributes: true, childList: true}));
};

/**
 * TODO(mtomasz): Merge the method into initAdditionalUI if possible.
 * @param {!Banners} banners
 */
FileManagerUI.prototype.initBanners = function(banners) {
  this.banners = banners;
  this.banners.addEventListener('relayout', this.relayout.bind(this));
};

/**
 * Relayouts the UI.
 */
FileManagerUI.prototype.relayout = function() {
  this.locationLine.truncate();
  // May not be available during initialization.
  if (this.listContainer.currentListType !==
      ListContainer.ListType.UNINITIALIZED) {
    this.listContainer.currentView.relayout();
  }
  if (this.directoryTree)
    this.directoryTree.relayout();
};

/**
 * Sets the current list type.
 * @param {ListContainer.ListType} listType New list type.
 */
FileManagerUI.prototype.setCurrentListType = function(listType) {
  this.listContainer.setCurrentListType(listType);

  var isListView = (listType === ListContainer.ListType.DETAIL);
  this.toggleViewButton.classList.toggle('thumbnail', isListView);

  var label = isListView ? str('CHANGE_TO_THUMBNAILVIEW_BUTTON_LABEL') :
                           str('CHANGE_TO_LISTVIEW_BUTTON_LABEL');
  this.toggleViewButton.setAttribute('aria-label', label);
  this.relayout();
};

/**
 * Overrides default handling for clicks on hyperlinks.
 * In a packaged apps links with targer='_blank' open in a new tab by
 * default, other links do not open at all.
 *
 * @param {!Event} event Click event.
 * @private
 */
FileManagerUI.prototype.onExternalLinkClick_ = function(event) {
  if (event.target.tagName != 'A' || !event.target.href)
    return;

  if (this.dialogType_ != DialogType.FULL_PAGE)
    this.dialogFooter.cancelButton.click();
};

/**
 * Re-focuses an element.
 * @private
 */
FileManagerUI.prototype.onFocusOut_ = function() {
  setTimeout(function() {
    // When there is no focus, the active element is the <body>
    if (document.activeElement !== document.body)
      return;

    var targetElement;
    if (this.dialogType_ == DialogType.SELECT_SAVEAS_FILE) {
      targetElement = this.dialogFooter.filenameInput;
    } else if (this.listContainer.currentListType !=
               ListContainer.ListType.UNINITIALIZED) {
      targetElement = this.listContainer.currentList;
    } else {
      return;
    }

    // Hack: if the tabIndex is disabled, we can assume a modal dialog is
    // shown. Focus to a button on the dialog instead.
    if (!targetElement.hasAttribute('tabIndex') || targetElement.tabIndex == -1)
      targetElement = document.querySelector('button:not([tabIndex="-1"])');

    if (targetElement)
      targetElement.focus();
  }.bind(this), 0);
};

/**
 * Decorates the given splitter element.
 * @param {!HTMLElement} splitterElement
 * @private
 */
FileManagerUI.prototype.decorateSplitter_ = function(splitterElement) {
  var self = this;
  var Splitter = cr.ui.Splitter;
  var customSplitter = cr.ui.define('div');

  customSplitter.prototype = {
    __proto__: Splitter.prototype,

    handleSplitterDragStart: function(e) {
      Splitter.prototype.handleSplitterDragStart.apply(this, arguments);
      this.ownerDocument.documentElement.classList.add('col-resize');
    },

    handleSplitterDragMove: function(deltaX) {
      Splitter.prototype.handleSplitterDragMove.apply(this, arguments);
      self.relayout();
    },

    handleSplitterDragEnd: function(e) {
      Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
      this.ownerDocument.documentElement.classList.remove('col-resize');
    }
  };

  customSplitter.decorate(splitterElement);
};
