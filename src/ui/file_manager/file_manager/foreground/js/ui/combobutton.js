// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a combobutton control.
 */
cr.define('cr.ui', () => {
  /**
   * Creates a new combo button element.
   */
  class ComboButton extends cr.ui.MultiMenuButton {
    constructor() {
      super();

      /** @private {?cr.ui.MenuItem} */
      this.defaultItem_ = null;

      /** @private {?Element} */
      this.trigger_ = null;

      /** @private {?Element} */
      this.actionNode_ = null;
    }

    /**
     * Truncates drop-down list.
     */
    clear() {
      this.menu.clear();
      this.multiple = false;
    }

    addDropDownItem(item) {
      this.multiple = true;
      const menuitem = this.menu.addMenuItem(item);

      // If menu is files-menu, decorate menu item as FilesMenuItem.
      if (this.menu.classList.contains('files-menu')) {
        cr.ui.decorate(menuitem, cr.ui.FilesMenuItem);
      }

      menuitem.data = item;
      if (item.iconType) {
        menuitem.style.backgroundImage = '';
        menuitem.setAttribute('file-type-icon', item.iconType);
      }
      if (item.bold) {
        menuitem.style.fontWeight = 'bold';
      }
      return menuitem;
    }

    /**
     * Adds separator to drop-down list.
     */
    addSeparator() {
      this.menu.addSeparator();
    }

    /**
     * Default item to fire on combobox click
     */
    get defaultItem() {
      return this.defaultItem_;
    }
    setDefaultItem_(defaultItem) {
      this.defaultItem_ = defaultItem;
      this.actionNode_.textContent = defaultItem.label || '';
    }
    set defaultItem(defaultItem) {
      this.setDefaultItem_(defaultItem);
    }

    /**
     * Utility function to set a boolean property get/setter.
     * @param {string} property Name of the property.
     */
    addBooleanProperty_(property) {
      Object.defineProperty(this, property, {
        get() {
          return this.getAttribute(property);
        },
        set(value) {
          if (value) {
            this.setAttribute(property, property);
          } else {
            this.removeAttribute(property);
          }
        },
        enumerable: true,
        configurable: true
      });
    }

    /**
     * cr.ui.decorate expects a static |decorate| method.
     *
     * @param {!Element} el Element to be decorated.
     * @return {!cr.ui.ComboButton} Decorated element.
     * @public
     */
    static decorate(el) {
      // Add the ComboButton methods to the element we're
      // decorating, leaving it's prototype chain intact.
      // Don't copy 'constructor' or property get/setters.
      Object.getOwnPropertyNames(ComboButton.prototype).forEach(name => {
        if (name !== 'constructor' && name !== 'multiple' &&
            name !== 'disabled') {
          el[name] = ComboButton.prototype[name];
        }
      });
      Object.getOwnPropertyNames(cr.ui.MultiMenuButton.prototype)
          .forEach(name => {
            if (name !== 'constructor' &&
                !Object.getOwnPropertyDescriptor(el, name)) {
              el[name] = cr.ui.MultiMenuButton.prototype[name];
            }
          });
      // Set up the 'menu, defaultItem, multiple and disabled'
      // properties & setter/getters.
      Object.defineProperty(el, 'menu', {
        get() {
          return this.menu_;
        },
        set(menu) {
          this.setMenu_(menu);
        },
        enumerable: true,
        configurable: true
      });
      Object.defineProperty(el, 'defaultItem', {
        get() {
          return this.defaultItem_;
        },
        set(defaultItem) {
          this.setDefaultItem_(defaultItem);
        },
        enumerable: true,
        configurable: true
      });
      el.addBooleanProperty_('multiple');
      el.addBooleanProperty_('disabled');
      el = /** @type {!cr.ui.ComboButton} */ (el);
      el.decorate();
      return el;
    }

    /**
     * Initializes the element.
     */
    decorate() {
      cr.ui.MultiMenuButton.prototype.decorate.call(this);

      this.classList.add('combobutton');

      const buttonLayer = this.ownerDocument.createElement('div');
      buttonLayer.classList.add('button');
      this.appendChild(buttonLayer);

      this.actionNode_ = this.ownerDocument.createElement('div');
      this.actionNode_.classList.add('action');
      buttonLayer.appendChild(this.actionNode_);

      const triggerIcon = this.ownerDocument.createElement('iron-icon');
      triggerIcon.setAttribute('icon', 'files:arrow-drop-down');
      this.trigger_ = this.ownerDocument.createElement('div');
      this.trigger_.classList.add('trigger');
      this.trigger_.appendChild(triggerIcon);

      buttonLayer.appendChild(this.trigger_);

      const ripplesLayer = this.ownerDocument.createElement('div');
      ripplesLayer.classList.add('ripples');
      this.appendChild(ripplesLayer);
      if (util.isFilesNg()) {
        ripplesLayer.setAttribute('hidden', '');
      }

      /** @private {!FilesToggleRipple} */
      this.filesToggleRipple_ = /** @type {!FilesToggleRipple} */
          (this.ownerDocument.createElement('files-toggle-ripple'));
      ripplesLayer.appendChild(this.filesToggleRipple_);

      /** @private {!PaperRipple} */
      this.paperRipple_ = /** @type {!PaperRipple} */
          (this.ownerDocument.createElement('paper-ripple'));
      ripplesLayer.appendChild(this.paperRipple_);

      this.addEventListener('click', this.handleButtonClick_.bind(this));
      this.addEventListener('menushow', this.handleMenuShow_.bind(this));
      this.addEventListener('menuhide', this.handleMenuHide_.bind(this));

      this.trigger_.addEventListener(
          'click', this.handleTriggerClicked_.bind(this));

      this.menu.addEventListener(
          'activate', this.handleMenuActivate_.bind(this));

      // Remove mousedown event listener created by MultiMenuButton::decorate,
      // and move it down to trigger_.
      this.removeEventListener('mousedown', this);
      this.trigger_.addEventListener('mousedown', this);
    }

    /**
     * Handles the keydown event for the menu button.
     */
    handleKeyDown(e) {
      switch (e.key) {
        case 'ArrowDown':
        case 'ArrowUp':
          if (!this.isMenuShown()) {
            this.showMenu(false);
          }
          e.preventDefault();
          break;
        case 'Escape':  // Maybe this is remote desktop playing a prank?
          this.hideMenu();
          break;
      }
    }

    handleTriggerClicked_(event) {
      event.stopPropagation();
    }

    handleMenuActivate_(event) {
      this.dispatchSelectEvent(event.target.data);
    }

    handleButtonClick_(event) {
      if (this.multiple) {
        // When there are multiple choices just show/hide menu.
        if (this.isMenuShown()) {
          this.hideMenu();
        } else {
          this.showMenu(true);
        }
      } else {
        // When there is only 1 choice, just dispatch to open.
        this.paperRipple_.simulatedRipple();
        this.blur();
        this.dispatchSelectEvent(this.defaultItem_);
      }
    }

    handleMenuShow_() {
      this.filesToggleRipple_.activated = true;
    }

    handleMenuHide_() {
      this.filesToggleRipple_.activated = false;
    }

    dispatchSelectEvent(item) {
      const selectEvent = new Event('select');
      selectEvent.item = item;
      this.dispatchEvent(selectEvent);
    }
  }

  cr.defineProperty(ComboButton, 'disabled', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(ComboButton, 'multiple', cr.PropertyKind.BOOL_ATTR);

  return {
    ComboButton: ComboButton,
  };
});
