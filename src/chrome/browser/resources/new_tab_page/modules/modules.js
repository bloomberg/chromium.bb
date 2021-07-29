// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';
import {$$} from '../utils.js';

import {Module} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {ModuleWrapperElement} from './module_wrapper.js';

/**
 * Container for the NTP modules.
 * @polymer
 * @extends {PolymerElement}
 */
export class ModulesElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-modules';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<string>} */
      dismissedModules_: {
        type: Array,
        value: () => [],
      },

      /** @private {!{all: boolean, ids: !Array<string>}} */
      disabledModules_: {
        type: Object,
        value: () => ({all: true, ids: []}),
      },

      /**
       * Data about the most recently removed module.
       * @type {?{message: string, undo: function()}}
       * @private
       */
      removedModuleData_: {
        type: Object,
        value: null,
      },

      /** @private */
      modulesLoaded_: Boolean,

      /** @private */
      modulesVisibilityDetermined_: Boolean,

      /** @private */
      modulesLoadedAndVisibilityDetermined_: {
        type: Boolean,
        computed: `computeModulesLoadedAndVisibilityDetermined_(
          modulesLoaded_,
          modulesVisibilityDetermined_)`,
        observer: 'onModulesLoadedAndVisibilityDeterminedChange_',
      },

      /** @private */
      dragEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('modulesDragAndDropEnabled'),
      },
    };
  }

  static get observers() {
    return ['onRemovedModulesChange_(dismissedModules_.*, disabledModules_)'];
  }

  constructor() {
    super();
    /** @private {?number} */
    this.setDisabledModulesListenerId_ = null;
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setDisabledModulesListenerId_ =
        NewTabPageProxy.getInstance()
            .callbackRouter.setDisabledModules.addListener((all, ids) => {
              this.disabledModules_ = {all, ids};
              this.modulesVisibilityDetermined_ = true;
            });
    NewTabPageProxy.getInstance().handler.updateDisabledModules();
    this.eventTracker_.add(window, 'keydown', e => this.onWindowKeydown_(e));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    NewTabPageProxy.getInstance().callbackRouter.removeListener(
        assert(this.setDisabledModulesListenerId_));
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    this.renderModules_();
  }

  /**
   * @return {!Promise<void>}
   * @private
   */
  async renderModules_() {
    const modules = await ModuleRegistry.getInstance().initializeModules(
        loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      NewTabPageProxy.getInstance().handler.onModulesLoadedWithData();
      modules.forEach(module => {
        const moduleWrapper = new ModuleWrapperElement();
        moduleWrapper.module = module;
        moduleWrapper.setAttribute('draggable', this.dragEnabled_);
        moduleWrapper.addEventListener('dragstart', event => {
          this.onDragStart_(/** @type {!DragEvent} */ (event));
        });
        moduleWrapper.addEventListener('dismiss-module', event => {
          this.onDismissModule_(
              /**
                 @type {!CustomEvent<{message: string, restoreCallback:
                     function()}>}
               */
              (event));
        });
        moduleWrapper.addEventListener('disable-module', event => {
          this.onDisableModule_(
              /**
                 @type {!CustomEvent<{message: string, restoreCallback:
                     ?function()}>}
               */
              (event));
        });
        moduleWrapper.hidden = this.moduleDisabled_(module.descriptor.id);

        const moduleContainer = this.ownerDocument.createElement('div');
        moduleContainer.classList.add('module-container');
        moduleContainer.appendChild(moduleWrapper);
        this.$.modules.appendChild(moduleContainer);
      });
      this.onModulesLoaded_();
    }
  }

  /**
   * @param {KeyboardEvent} e
   * @private
   */
  onWindowKeydown_(e) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoRemoveModuleButtonClick_();
    }
  }

  /** @private */
  onModulesLoaded_() {
    this.modulesLoaded_ = true;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeModulesLoadedAndVisibilityDetermined_() {
    return this.modulesLoaded_ && this.modulesVisibilityDetermined_;
  }

  /** @private */
  onModulesLoadedAndVisibilityDeterminedChange_() {
    if (this.modulesLoadedAndVisibilityDetermined_) {
      this.shadowRoot.querySelectorAll('ntp-module-wrapper')
          .forEach(({module}) => {
            chrome.metricsPrivate.recordBoolean(
                `NewTabPage.Modules.EnabledOnNTPLoad.${module.descriptor.id}`,
                !this.disabledModules_.all &&
                    !this.disabledModules_.ids.includes(module.descriptor.id));
          });
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.Modules.VisibleOnNTPLoad', !this.disabledModules_.all);
      this.dispatchEvent(new Event('modules-loaded'));
    }
  }

  /**
   * @param {!CustomEvent<{message: string, restoreCallback: function()}>} e
   *     Event notifying a module was dismissed. Contains the message to show in
   *     the toast.
   * @private
   */
  onDismissModule_(e) {
    const id =
        /** @type {ModuleWrapperElement} */ (e.target).module.descriptor.id;
    const restoreCallback = e.detail.restoreCallback;
    this.removedModuleData_ = {
      message: e.detail.message,
      undo: () => {
        this.splice('dismissedModules_', this.dismissedModules_.indexOf(id), 1);
        restoreCallback();
        NewTabPageProxy.getInstance().handler.onRestoreModule(id);
      },
    };
    if (!this.dismissedModules_.includes(id)) {
      this.push('dismissedModules_', id);
    }

    // Notify the user.
    $$(this, '#removeModuleToast').show();
    // Notify the backend.
    NewTabPageProxy.getInstance().handler.onDismissModule(id);
  }

  /**
   * @param {!CustomEvent<{message: string, restoreCallback: ?function()}>} e
   *     Event notifying a module was disabled. Contains the message to show in
   *     the toast.
   * @private
   */
  onDisableModule_(e) {
    const id =
        /** @type {ModuleWrapperElement} */ (e.target).module.descriptor.id;
    const restoreCallback = e.detail.restoreCallback;
    this.removedModuleData_ = {
      message: e.detail.message,
      undo: () => {
        if (restoreCallback) {
          restoreCallback();
        }
        NewTabPageProxy.getInstance().handler.setModuleDisabled(id, false);
        chrome.metricsPrivate.recordSparseHashable(
            'NewTabPage.Modules.Enabled', id);
        chrome.metricsPrivate.recordSparseHashable(
            'NewTabPage.Modules.Enabled.Toast', id);
      },
    };

    NewTabPageProxy.getInstance().handler.setModuleDisabled(id, true);
    $$(this, '#removeModuleToast').show();
    chrome.metricsPrivate.recordSparseHashable(
        'NewTabPage.Modules.Disabled', id);
    chrome.metricsPrivate.recordSparseHashable(
        'NewTabPage.Modules.Disabled.ModuleRequest', id);
  }

  /**
   * @param {string} id
   * @return {boolean}
   * @private
   */
  moduleDisabled_(id) {
    return this.disabledModules_.all || this.dismissedModules_.includes(id) ||
        this.disabledModules_.ids.includes(id);
  }

  /** @private */
  onUndoRemoveModuleButtonClick_() {
    if (!this.removedModuleData_) {
      return;
    }

    // Restore the module.
    this.removedModuleData_.undo();

    // Notify the user.
    $$(this, '#removeModuleToast').hide();

    this.removedModuleData_ = null;
  }

  /**
   * Hides and reveals modules depending on removed status.
   * @private
   */
  onRemovedModulesChange_() {
    this.shadowRoot.querySelectorAll('ntp-module-wrapper')
        .forEach(moduleWrapper => {
          moduleWrapper.hidden =
              this.moduleDisabled_(moduleWrapper.module.descriptor.id);
        });
  }

  /**
   * Module is dragged by updating the module position based on the
   * position of the pointer.
   * @param {!DragEvent} e
   * @private
   */
  onDragStart_(e) {
    assert(loadTimeData.getBoolean('modulesDragAndDropEnabled'));

    // |dataTransfer| is null in tests.
    if (e.dataTransfer) {
      // Remove the transparent image that appears on top when dragging.
      e.dataTransfer.setDragImage(new Image(), 0, 0);
      e.dataTransfer.effectAllowed = 'move';
    }

    const dragElement = e.target;
    const dragElementRect = dragElement.getBoundingClientRect();
    // This is the offset between the pointer and module so that the
    // module isn't dragged by the top-left corner.
    const dragOffset = {
      x: e.x - dragElementRect.x,
      y: e.y - dragElementRect.y,
    };

    dragElement.parentElement.style.width = `${dragElementRect.width}px`;
    dragElement.parentElement.style.height = `${dragElementRect.height}px`;

    const dragOver = e => {
      e.preventDefault();
      if (e.dataTransfer) {
        e.dataTransfer.dropEffect = 'move';
      }

      dragElement.setAttribute('dragging', '');
      dragElement.style.left = `${e.x - dragOffset.x}px`;
      dragElement.style.top = `${e.y - dragOffset.y}px`;
    };

    const dragEnter = e => {
      const moduleContainers = Array.from(this.$.modules.childNodes);
      const dragIndex = moduleContainers.indexOf(dragElement.parentElement);
      const dropIndex = moduleContainers.indexOf(e.target.parentElement);

      const positionType = dragIndex > dropIndex ? 'beforebegin' : 'afterend';
      const dragContainer = moduleContainers[dragIndex];
      const previousContainer = moduleContainers[dropIndex];

      dragContainer.remove();
      previousContainer.insertAdjacentElement(positionType, dragContainer);
    };

    const undraggedModuleWrappers =
        Array.from(this.shadowRoot.querySelectorAll('ntp-module-wrapper'))
            .filter(moduleWrapper => moduleWrapper !== dragElement);

    undraggedModuleWrappers.forEach(moduleWrapper => {
      moduleWrapper.addEventListener('dragenter', dragEnter);
    });

    this.ownerDocument.addEventListener('dragover', dragOver);
    this.ownerDocument.addEventListener('dragend', () => {
      this.ownerDocument.removeEventListener('dragover', dragOver);

      undraggedModuleWrappers.forEach(moduleWrapper => {
        moduleWrapper.removeEventListener('dragenter', dragEnter);
      });

      dragElement.removeAttribute('dragging');
      dragElement.style.removeProperty('left');
      dragElement.style.removeProperty('top');
    }, {once: true});
  }
}

customElements.define(ModulesElement.is, ModulesElement);
