// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {decorate, define as crUiDefine} from 'chrome://resources/js/cr/ui.m.js';
import {contextMenuHandler} from 'chrome://resources/js/cr/ui/context_menu_handler.m.js';
import {BaseDialog} from 'chrome://resources/js/cr/ui/dialogs.m.js';
import {Menu} from 'chrome://resources/js/cr/ui/menu.m.js';
import {MenuItem} from 'chrome://resources/js/cr/ui/menu_item.m.js';
import {Splitter} from 'chrome://resources/js/cr/ui/splitter.js';
import {queryRequiredElement} from 'chrome://resources/js/util.m.js';

import {DialogType} from '../../../common/js/dialog_type.js';
import {str, strf, util} from '../../../common/js/util.js';
import {AllowedPaths} from '../../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {FilesPasswordDialog} from '../../elements/files_password_dialog.js';
import {FilesToast} from '../../elements/files_toast.js';
import {FilesTooltip} from '../../elements/files_tooltip.js';
import {BannerController} from '../banner_controller.js';
import {LaunchParam} from '../launch_param.js';
import {ProvidersModel} from '../providers_model.js';

import {A11yAnnounce} from './a11y_announce.js';
import {ActionModelUI} from './action_model_ui.js';
import {ActionsSubmenu} from './actions_submenu.js';
import {ComboButton} from './combobutton.js';
import {DefaultTaskDialog} from './default_task_dialog.js';
import {DialogFooter} from './dialog_footer.js';
import {DirectoryTree} from './directory_tree.js';
import {FileGrid} from './file_grid.js';
import {FileTable} from './file_table.js';
import {FilesAlertDialog} from './files_alert_dialog.js';
import {FilesConfirmDialog} from './files_confirm_dialog.js';
import {FilesMenuItem} from './files_menu.js';
import {GearMenu} from './gear_menu.js';
import {ImportCrostiniImageDialog} from './import_crostini_image_dialog.js';
import {InstallLinuxPackageDialog} from './install_linux_package_dialog.js';
import {ListContainer} from './list_container.js';
import {LocationLine} from './location_line.js';
import {MultiMenu} from './multi_menu.js';
import {MultiMenuButton} from './multi_menu_button.js';
import {ProgressCenterPanel} from './progress_center_panel.js';
import {ProvidersMenu} from './providers_menu.js';
import {SearchBox} from './search_box.js';


/**
 * The root of the file manager's view managing the DOM of the Files app.
 * @implements {ActionModelUI}
 * @implements {A11yAnnounce}
 */
export class FileManagerUI {
  /**
   * @param {!ProvidersModel} providersModel Model for providers.
   * @param {!HTMLElement} element Top level element of the Files app.
   * @param {!LaunchParam} launchParam Launch param.
   */
  constructor(providersModel, element, launchParam) {
    // Initialize the dialog label. This should be done before constructing
    // dialog instances.
    BaseDialog.OK_LABEL = str('OK_LABEL');
    BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');

    /**
     * Top level element of the Files app.
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
     * Alert dialog.
     * @type {!FilesAlertDialog}
     * @const
     */
    this.alertDialog = new FilesAlertDialog(this.element);

    /**
     * Confirm dialog.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.confirmDialog = new FilesConfirmDialog(this.element);

    /**
     * Confirm dialog for delete.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.deleteConfirmDialog = new FilesConfirmDialog(this.element);
    this.deleteConfirmDialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
    this.deleteConfirmDialog.focusCancelButton = true;

    /**
     * Confirm dialog for file move operation.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.moveConfirmDialog = new FilesConfirmDialog(this.element);
    this.moveConfirmDialog.setOkLabel(str('CONFIRM_MOVE_BUTTON_LABEL'));

    /**
     * Confirm dialog for file copy operation.
     * @type {!FilesConfirmDialog}
     * @const
     */
    this.copyConfirmDialog = new FilesConfirmDialog(this.element);
    this.copyConfirmDialog.setOkLabel(str('CONFIRM_COPY_BUTTON_LABEL'));

    /**
     * Default task picker.
     * @type {!DefaultTaskDialog}
     * @const
     */
    this.defaultTaskPicker = new DefaultTaskDialog(this.element);

    /**
     * Dialog for installing .deb files
     * @type {!InstallLinuxPackageDialog}
     * @const
     */
    this.installLinuxPackageDialog =
        new InstallLinuxPackageDialog(this.element);

    /**
     * Dialog for import Crostini Image Files (.tini)
     * @type {!ImportCrostiniImageDialog}
     * @const
     */
    this.importCrostiniImageDialog =
        new ImportCrostiniImageDialog(this.element);

    /**
     * Dialog for formatting
     * @const {!HTMLElement}
     */
    this.formatDialog = queryRequiredElement('#format-dialog');

    /**
     * Dialog for password prompt
     * @type {?FilesPasswordDialog}
     */
    this.passwordDialog_ = null;

    /**
     * The container element of the dialog.
     * @type {!HTMLElement}
     */
    this.dialogContainer =
        queryRequiredElement('.dialog-container', this.element);
    this.dialogContainer.addEventListener('relayout', (event) => {
      this.layoutChanged_();
    });

    /**
     * Context menu for texts.
     * @type {!Menu}
     * @const
     */
    this.textContextMenu =
        util.queryDecoratedElement('#text-context-menu', Menu);

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
    this.toolbar = queryRequiredElement('.dialog-header', this.element);

    /**
     * The actionbar which contains buttons to perform actions on selected
     * file(s).
     * @type {!HTMLElement}
     * @const
     */
    this.actionbar = queryRequiredElement('#action-bar', this.toolbar);

    /**
     * The navigation list.
     * @type {!HTMLElement}
     * @const
     */
    this.dialogNavigationList =
        queryRequiredElement('.dialog-navigation-list', this.element);

    /**
     * Search box.
     * @type {!SearchBox}
     * @const
     */
    this.searchBox = new SearchBox(
        queryRequiredElement('#search-box', this.element),
        queryRequiredElement('#search-wrapper', this.element),
        queryRequiredElement('#search-button', this.element));

    /**
     * Toggle-view button.
     * @type {!Element}
     * @const
     */
    this.toggleViewButton = queryRequiredElement('#view-button', this.element);

    /**
     * The button to sort the file list.
     * @type {!MultiMenuButton}
     * @const
     */
    this.sortButton =
        util.queryDecoratedElement('#sort-button', MultiMenuButton);

    /**
     * Ripple effect of sort button.
     * @type {!FilesToggleRippleElement}
     * @const
     */
    this.sortButtonToggleRipple =
        /** @type {!FilesToggleRippleElement} */ (
            queryRequiredElement('files-toggle-ripple', this.sortButton));

    /**
     * The button to open gear menu.
     * @type {!MultiMenuButton}
     * @const
     */
    this.gearButton =
        util.queryDecoratedElement('#gear-button', MultiMenuButton);

    /**
     * Ripple effect of gear button.
     * @type {!FilesToggleRippleElement}
     * @const
     */
    this.gearButtonToggleRipple =
        /** @type {!FilesToggleRippleElement} */ (
            queryRequiredElement('files-toggle-ripple', this.gearButton));

    /**
     * @type {!GearMenu}
     * @const
     */
    this.gearMenu = new GearMenu(this.gearButton.menu);

    /**
     * The button to open context menu in the check-select mode.
     * @type {!MultiMenuButton}
     * @const
     */
    this.selectionMenuButton =
        util.queryDecoratedElement('#selection-menu-button', MultiMenuButton);

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
    this.progressCenterPanel = new ProgressCenterPanel();

    /**
     * Activity feedback panel.
     * @type {!HTMLElement}
     * @const
     */
    this.activityProgressPanel =
        queryRequiredElement('#progress-panel', this.element);

    /**
     * List container.
     * @type {!ListContainer}
     */
    this.listContainer;

    /**
     * @type {!MultiMenu}
     * @const
     */
    this.fileContextMenu =
        util.queryDecoratedElement('#file-context-menu', MultiMenu);

    /**
     * @public {!FilesMenuItem}
     * @const
     */
    this.defaultTaskMenuItem =
        /** @type {!FilesMenuItem} */
        (queryRequiredElement('#default-task-menu-item', this.fileContextMenu));

    /**
     * @public @const {!MenuItem}
     */
    this.tasksSeparator = /** @type {!MenuItem} */
        (queryRequiredElement('#tasks-separator', this.fileContextMenu));

    /**
     * The combo button to specify the task.
     * @type {!ComboButton}
     * @const
     */
    this.taskMenuButton = util.queryDecoratedElement('#tasks', ComboButton);
    this.taskMenuButton.showMenu = function(shouldSetFocus) {
      // Prevent the empty menu from opening.
      if (!this.menu.length) {
        return;
      }
      ComboButton.prototype.showMenu.call(this, shouldSetFocus);
    };

    /**
     * Banners in the file list.
     * @type {BannerController}
     */
    this.banners = null;

    /**
     * Dialog footer.
     * @type {!DialogFooter}
     */
    this.dialogFooter = DialogFooter.findDialogFooter(
        this.dialogType_,
        /** @type {!Document} */ (this.element.ownerDocument));

    /**
     * @public {!ProvidersMenu}
     * @const
     */
    this.providersMenu = new ProvidersMenu(
        providersModel, util.queryDecoratedElement('#providers-menu', Menu));

    /**
     * @public {!ActionsSubmenu}
     * @const
     */
    this.actionsSubmenu = new ActionsSubmenu(this.fileContextMenu);

    /**
     * @type {!FilesToast}
     * @const
     */
    this.toast =
        /** @type {!FilesToast} */ (document.querySelector('files-toast'));

    /**
     * Container of file-type filter buttons.
     * @const {!HTMLElement}
     */
    this.fileTypeFilterContainer =
        queryRequiredElement('#file-type-filter-container', this.element);

    /**
     * A hidden div that can be used to announce text to screen
     * reader/ChromeVox.
     * @private {!HTMLElement}
     */
    this.a11yMessage_ = queryRequiredElement('#a11y-msg', this.element);

    if (window.IN_TEST) {
      /**
       * Stores all a11y announces to be checked in tests.
       * @public {Array<string>}
       */
      this.a11yAnnounces = [];
    }

    // Initialize attributes.
    this.element.setAttribute('type', this.dialogType_);
    if (launchParam.allowedPaths !== AllowedPaths.ANY_PATH_OR_URL) {
      this.element.setAttribute('block-hosted-docs', '');
    }

    // Modify UI default behavior.
    this.element.addEventListener(
        'click', this.onExternalLinkClick_.bind(this));
    this.element.addEventListener('drop', e => {
      e.preventDefault();
    });
    if (util.runningInBrowser()) {
      this.element.addEventListener('contextmenu', e => {
        e.preventDefault();
        e.stopPropagation();
      });
    }

    /**
     * True while FilesApp is in the process of a drag and drop. Set to true on
     * 'dragstart', set to false on 'dragend'. If CrostiniEvent
     * 'drop_failed_plugin_vm_directory_not_shared' is received during drag, we
     * show the move-to-windows-files dialog.
     *
     * @public {boolean}
     */
    this.dragInProcess = false;
  }

  /**
   * Gets password dialog.
   * @return {!Element}
   */
  get passwordDialog() {
    if (this.passwordDialog_) {
      return this.passwordDialog_;
    }
    this.passwordDialog_ = /** @type {!FilesPasswordDialog} */ (
        document.createElement('files-password-dialog'));
    this.element.appendChild(this.passwordDialog_);
    return this.passwordDialog_;
  }

  /**
   * Initializes here elements, which are expensive or hidden in the beginning.
   *
   * @param {!FileTable} table
   * @param {!FileGrid} grid
   * @param {!VolumeManager} volumeManager
   */
  initAdditionalUI(table, grid, volumeManager) {
    // List container.
    this.listContainer = new ListContainer(
        queryRequiredElement('#list-container', this.element), table, grid,
        this.dialogType_);

    // Location line.
    this.locationLine = new LocationLine(
        queryRequiredElement('#location-breadcrumbs', this.element),
        volumeManager, this.listContainer);

    // Splitter.
    this.decorateSplitter_(
        queryRequiredElement('#navigation-list-splitter', this.element));

    // Init context menus.
    contextMenuHandler.setContextMenu(grid, this.fileContextMenu);
    contextMenuHandler.setContextMenu(table.list, this.fileContextMenu);
    contextMenuHandler.setContextMenu(
        queryRequiredElement('.drive-welcome.page'), this.fileContextMenu);

    // Add window resize handler.
    document.defaultView.addEventListener('resize', this.relayout.bind(this));

    // Add global pointer-active handler.
    const rootElement = document.documentElement;
    let pointerActive = ['pointerdown', 'pointerup', 'dragend', 'touchend'];
    if (window.IN_TEST) {
      pointerActive = pointerActive.concat(['mousedown', 'mouseup']);
    }
    pointerActive.forEach((eventType) => {
      document.addEventListener(eventType, (e) => {
        rootElement.classList.toggle('pointer-active', /down$/.test(e.type));
      }, true);
    });

    // Add global drag-drop-active handler.
    let activeDropTarget = null;
    ['dragenter', 'dragleave', 'drop'].forEach((eventType) => {
      document.addEventListener(eventType, (event) => {
        const dragDropActive = 'drag-drop-active';
        if (event.type === 'dragenter') {
          rootElement.classList.add(dragDropActive);
          activeDropTarget = event.target;
        } else if (activeDropTarget === event.target) {
          rootElement.classList.remove(dragDropActive);
          activeDropTarget = null;
        }
      });
    });

    document.addEventListener('dragstart', () => {
      this.dragInProcess = true;
    });
    document.addEventListener('dragend', () => {
      this.dragInProcess = false;
    });

    // Observe the dialog header content box size: the breadcrumb and action
    // bar buttons can become wide enough to extend past the available viewport,
    // and this.layoutChanged_() is used to clamp their size to the viewport.
    const resizeObserver = new ResizeObserver(() => this.layoutChanged_());
    resizeObserver.observe(queryRequiredElement('div.dialog-header'));
  }

  /**
   * Initializes the focus.
   */
  initUIFocus() {
    // Set the initial focus. When there is no focus, the active element is the
    // <body>.
    let targetElement = null;
    if (this.dialogType_ == DialogType.SELECT_SAVEAS_FILE) {
      targetElement = this.dialogFooter.filenameInput;
    } else if (
        this.listContainer.currentListType !=
        ListContainer.ListType.UNINITIALIZED) {
      targetElement = this.listContainer.currentList;
    }

    if (targetElement) {
      targetElement.focus();
    }
  }

  /**
   * TODO(hirono): Merge the method into initAdditionalUI.
   * @param {!DirectoryTree} directoryTree
   */
  initDirectoryTree(directoryTree) {
    this.directoryTree = directoryTree;

    // Set up the context menu for the volume/shortcut items in directory tree.
    this.directoryTree.contextMenuForRootItems =
        util.queryDecoratedElement('#roots-context-menu', Menu);
    this.directoryTree.contextMenuForSubitems =
        util.queryDecoratedElement('#directory-tree-context-menu', Menu);
    this.directoryTree.disabledContextMenu =
        util.queryDecoratedElement('#disabled-context-menu', Menu);
  }

  /**
   * TODO(mtomasz): Merge the method into initAdditionalUI if possible.
   * @param {!BannerController} banners
   */
  initBanners(banners) {
    this.banners = banners;
    this.banners.addEventListener('relayout', this.relayout.bind(this));
  }

  /**
   * Attaches files tooltip.
   */
  attachFilesTooltip() {
    const filesTooltip =
        assertInstanceof(document.querySelector('files-tooltip'), FilesTooltip);
    filesTooltip.addTargets(document.querySelectorAll('[has-tooltip]'));

    this.locationLine.filesTooltip = filesTooltip;
  }

  /**
   * Initialize files menu items. This method must be called after all files
   * menu items are decorated as MenuItem.
   */
  decorateFilesMenuItems() {
    const filesMenuItems =
        document.querySelectorAll('cr-menu.files-menu > cr-menu-item');

    for (let i = 0; i < filesMenuItems.length; i++) {
      const filesMenuItem = filesMenuItems[i];
      assertInstanceof(filesMenuItem, MenuItem);
      decorate(filesMenuItem, FilesMenuItem);
    }
  }

  /**
   * Relayouts the UI.
   */
  relayout() {
    // May not be available during initialization.
    if (this.listContainer.currentListType !==
        ListContainer.ListType.UNINITIALIZED) {
      this.listContainer.currentView.relayout();
    }
    if (this.directoryTree) {
      this.directoryTree.relayout();
    }
  }

  /**
   * Handles the 'relayout' event to set sizing of the dialog main panel.
   *
   * @private
   */
  layoutChanged_() {
    if (this.scrollRAFActive_ === true) {
      return;
    }

    /**
     * True if a scroll RAF is active: scroll events are frequent and serviced
     * using RAF to throttle our processing of these events.
     * @type {boolean}
     */
    this.scrollRAFActive_ = true;

    window.requestAnimationFrame(() => {
      this.scrollRAFActive_ = false;

      const mainWindow = document.querySelector('.dialog-container');
      const navigationList = document.querySelector('.dialog-navigation-list');
      const splitter = document.querySelector('.splitter');
      const dialogMain = document.querySelector('.dialog-main');

      // Check the width of the tree and splitter and set the main panel width
      // to the remainder if it's too wide.
      const mainWindowWidth = mainWindow.offsetWidth;
      const navListWidth = navigationList.offsetWidth;
      const splitStyle = window.getComputedStyle(splitter);
      const splitMargin = parseInt(splitStyle.marginRight, 10) +
          parseInt(splitStyle.marginLeft, 10);
      const splitWidth = splitter.offsetWidth + splitMargin;
      const dialogMainWidth = dialogMain.offsetWidth;
      if (!dialogMain.style.width ||
          (navListWidth + splitWidth + dialogMainWidth) > mainWindowWidth) {
        dialogMain.style.width =
            (mainWindowWidth - navListWidth - splitWidth) + 'px';
      }
    });
  }

  /**
   * Sets the current list type.
   * @param {ListContainer.ListType} listType New list type.
   */
  setCurrentListType(listType) {
    this.listContainer.setCurrentListType(listType);

    const isListView = (listType === ListContainer.ListType.DETAIL);
    this.toggleViewButton.classList.toggle('thumbnail', isListView);

    const label = isListView ? str('CHANGE_TO_THUMBNAILVIEW_BUTTON_LABEL') :
                               str('CHANGE_TO_LISTVIEW_BUTTON_LABEL');
    this.toggleViewButton.setAttribute('aria-label', label);
    this.relayout();
  }

  /**
   * Overrides default handling for clicks on hyperlinks.
   * In a packaged apps links with target='_blank' open in a new tab by
   * default, other links do not open at all.
   *
   * @param {!Event} event Click event.
   * @private
   */
  onExternalLinkClick_(event) {
    if (event.target.tagName != 'A' || !event.target.href) {
      return;
    }

    if (this.dialogType_ != DialogType.FULL_PAGE) {
      this.dialogFooter.cancelButton.click();
    }
  }

  /**
   * Decorates the given splitter element.
   * @param {!HTMLElement} splitterElement
   * @param {boolean=} opt_resizeNextElement
   * @private
   */
  decorateSplitter_(splitterElement, opt_resizeNextElement) {
    const self = this;
    const FileSplitter = Splitter;
    const customSplitter = crUiDefine('div');

    customSplitter.prototype = {
      __proto__: FileSplitter.prototype,

      handleSplitterDragStart: function(e) {
        FileSplitter.prototype.handleSplitterDragStart.apply(this, arguments);
        this.ownerDocument.documentElement.classList.add('col-resize');
      },

      handleSplitterDragMove: function(deltaX) {
        FileSplitter.prototype.handleSplitterDragMove.apply(this, arguments);
        self.relayout();
      },

      handleSplitterDragEnd: function(e) {
        FileSplitter.prototype.handleSplitterDragEnd.apply(this, arguments);
        this.ownerDocument.documentElement.classList.remove('col-resize');
      }
    };

    /** @type Object */ (customSplitter).decorate(splitterElement);
    splitterElement.resizeNextElement = !!opt_resizeNextElement;
  }

  /**
   * Mark |element| with "loaded" attribute to indicate that File Manager has
   * finished loading.
   */
  addLoadedAttribute() {
    this.element.setAttribute('loaded', '');
  }

  /**
   * Sets up and shows the alert to inform a user the task is opened in the
   * desktop of the running profile.
   *
   * @param {Array<Entry>} entries List of opened entries.
   */
  showOpenInOtherDesktopAlert(entries) {
    if (!entries.length) {
      return;
    }
    chrome.fileManagerPrivate.getProfiles(
        (profiles, currentId, displayedId) => {
          // Find strings.
          let displayName;
          for (let i = 0; i < profiles.length; i++) {
            if (profiles[i].profileId === currentId) {
              displayName = profiles[i].displayName;
              break;
            }
          }
          if (!displayName) {
            console.warn('Display name is not found.');
            return;
          }

          const title = entries.length > 1 ?
              entries[0].name + '\u2026' /* ellipsis */ :
              entries[0].name;
          const message = strf(
              entries.length > 1 ? 'OPEN_IN_OTHER_DESKTOP_MESSAGE_PLURAL' :
                                   'OPEN_IN_OTHER_DESKTOP_MESSAGE',
              displayName, currentId);

          // Show the dialog.
          this.alertDialog.showWithTitle(title, message, null, null, null);
        });
  }

  /**
   * Shows confirmation dialog and handles user interaction.
   * @param {boolean} isMove true if the operation is move. false if copy.
   * @param {!Array<string>} messages The messages to show in the dialog.
   *     box.
   * @return {!Promise<boolean>}
   */
  showConfirmationDialog(isMove, messages) {
    let dialog = null;
    if (isMove) {
      dialog = this.moveConfirmDialog;
    } else {
      dialog = this.copyConfirmDialog;
    }
    return new Promise((resolve, reject) => {
      dialog.show(
          messages.join(' '),
          () => {
            resolve(true);
          },
          () => {
            resolve(false);
          });
    });
  }

  /**
   * Send a text to screen reader/Chromevox without displaying the text in the
   * UI.
   * @param {string} text Text to be announced by screen reader, which should be
   * already translated.
   */
  speakA11yMessage(text) {
    // Screen reader only reads if the content changes, so clear the content
    // first.
    this.a11yMessage_.textContent = '';
    this.a11yMessage_.textContent = text;
    if (window.IN_TEST) {
      this.a11yAnnounces.push(text);
    }
  }
}
