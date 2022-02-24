// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';

import {ModuleHeight} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {ModuleWrapperElement} from './module_wrapper.js';
import {getTemplate} from './modules.html.js';


export type DismissModuleEvent =
    CustomEvent<{message: string, restoreCallback: () => void}>;
export type DisableModuleEvent = DismissModuleEvent;

declare global {
  interface HTMLElementEventMap {
    'dismiss-module': DismissModuleEvent;
    'disable-module': DisableModuleEvent;
  }
}

export interface ModulesElement {
  $: {
    modules: HTMLElement,
    removeModuleToast: CrToastElement,
    removeModuleToastMessage: HTMLElement,
    undoRemoveModuleButton: HTMLElement,
  };
}

const SHORT_CLASS_NAME: string = 'short';
const TALL_CLASS_NAME: string = 'tall';

/** Container for the NTP modules. */
export class ModulesElement extends PolymerElement {
  static get is() {
    return 'ntp-modules';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dismissedModules_: {
        type: Array,
        value: () => [],
      },

      disabledModules_: {
        type: Object,
        value: () => ({all: true, ids: []}),
      },

      /** Data about the most recently removed module. */
      removedModuleData_: {
        type: Object,
        value: null,
      },

      modulesLoaded_: Boolean,
      modulesVisibilityDetermined_: Boolean,

      modulesLoadedAndVisibilityDetermined_: {
        type: Boolean,
        computed: `computeModulesLoadedAndVisibilityDetermined_(
          modulesLoaded_,
          modulesVisibilityDetermined_)`,
        observer: 'onModulesLoadedAndVisibilityDeterminedChange_',
      },

      dragEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesDragAndDropEnabled'),
        reflectToAttribute: true,
      },

      /** @private {boolean} */
      modulesRedesignedLayoutEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesRedesignedLayoutEnabled'),
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return ['onRemovedModulesChange_(dismissedModules_.*, disabledModules_)'];
  }

  private dismissedModules_: string[];
  private disabledModules_: {all: boolean, ids: string[]};
  private removedModuleData_: {message: string, undo: () => void}|null;
  private modulesLoaded_: boolean;
  private modulesVisibilityDetermined_: boolean;
  private modulesLoadedAndVisibilityDetermined_: boolean;
  private dragEnabled_: boolean;

  private setDisabledModulesListenerId_: number|null = null;
  private eventTracker_: EventTracker = new EventTracker();

  connectedCallback() {
    super.connectedCallback();
    this.setDisabledModulesListenerId_ =
        NewTabPageProxy.getInstance()
            .callbackRouter.setDisabledModules.addListener(
                (all: boolean, ids: string[]) => {
                  this.disabledModules_ = {all, ids};
                  this.modulesVisibilityDetermined_ = true;
                });
    NewTabPageProxy.getInstance().handler.updateDisabledModules();
    this.eventTracker_.add(window, 'keydown', this.onWindowKeydown_.bind(this));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    NewTabPageProxy.getInstance().callbackRouter.removeListener(
        assert(this.setDisabledModulesListenerId_!));
    this.eventTracker_.removeAll();
  }

  ready() {
    super.ready();
    this.renderModules_();
  }

  private appendModuleContainers_(moduleContainers: HTMLElement[]) {
    this.$.modules.innerHTML = '';
    let shortModuleSiblingsContainer: HTMLElement|null = null;
    moduleContainers.forEach((moduleContainer: HTMLElement, index: number) => {
      let moduleContainerParent = this.$.modules;
      if (loadTimeData.getBoolean('modulesRedesignedLayoutEnabled')) {
        // Wrap pairs of sibling short modules in a container. All other
        // modules will be placed in a container of their own.
        if (moduleContainer.classList.contains(SHORT_CLASS_NAME) &&
            shortModuleSiblingsContainer) {
          // Add current sibling short module to container which already
          // contains the previous sibling short module by setting its parent
          // to be 'shortModuleSiblingsContainer'.
          moduleContainerParent = shortModuleSiblingsContainer;
          this.$.modules.appendChild(shortModuleSiblingsContainer);
          shortModuleSiblingsContainer = null;
        } else if (
            moduleContainer.classList.contains(SHORT_CLASS_NAME) &&
            index + 1 !== moduleContainers.length &&
            moduleContainers[index + 1].classList.contains(SHORT_CLASS_NAME)) {
          // Add current short module to a new container since the next one is
          // short as well by setting its parent to be
          // 'shortModuleSiblingsContainer'.
          shortModuleSiblingsContainer =
              this.ownerDocument.createElement('div');
          shortModuleSiblingsContainer.classList.add(
              'short-module-siblings-container');
          moduleContainerParent = shortModuleSiblingsContainer;
        }
      }
      moduleContainerParent.appendChild(moduleContainer);
    });
  }

  private async renderModules_(): Promise<void> {
    const modules = await ModuleRegistry.getInstance().initializeModules(
        loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      NewTabPageProxy.getInstance().handler.onModulesLoadedWithData();
      const moduleContainers = modules.map(module => {
        const moduleWrapper = new ModuleWrapperElement();
        moduleWrapper.module = module;
        if (this.dragEnabled_) {
          moduleWrapper.addEventListener(
              'mousedown', e => this.onDragStart_(e));
        }
        if (!loadTimeData.getBoolean('modulesRedesignedEnabled')) {
          moduleWrapper.addEventListener(
              'dismiss-module', e => this.onDismissModule_(e));
        }
        moduleWrapper.addEventListener(
            'disable-module', e => this.onDisableModule_(e));
        const moduleContainer = this.ownerDocument.createElement('div');
        moduleContainer.classList.add('module-container');
        if (loadTimeData.getBoolean('modulesRedesignedLayoutEnabled')) {
          if (module.descriptor.height === ModuleHeight.SHORT) {
            moduleContainer.classList.add(SHORT_CLASS_NAME);
          }
          if (module.descriptor.height === ModuleHeight.TALL) {
            moduleContainer.classList.add(TALL_CLASS_NAME);
          }
        }
        moduleContainer.hidden = this.moduleDisabled_(module.descriptor.id);
        moduleContainer.appendChild(moduleWrapper);
        return moduleContainer;
      });
      this.appendModuleContainers_(moduleContainers);
      this.onModulesLoaded_();
    }
  }

  private onWindowKeydown_(e: KeyboardEvent) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoRemoveModuleButtonClick_();
    }
  }

  private onModulesLoaded_() {
    this.modulesLoaded_ = true;
  }

  private computeModulesLoadedAndVisibilityDetermined_(): boolean {
    return this.modulesLoaded_ && this.modulesVisibilityDetermined_;
  }

  /** @private */
  onModulesLoadedAndVisibilityDeterminedChange_() {
    if (this.modulesLoadedAndVisibilityDetermined_) {
      ModuleRegistry.getInstance().getDescriptors().forEach(({id}) => {
        chrome.metricsPrivate.recordBoolean(
            `NewTabPage.Modules.EnabledOnNTPLoad.${id}`,
            !this.disabledModules_.all &&
                !this.disabledModules_.ids.includes(id));
      });
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.Modules.VisibleOnNTPLoad', !this.disabledModules_.all);
      this.dispatchEvent(new Event('modules-loaded'));
    }
  }

  /**
   * @param e Event notifying a module was dismissed. Contains the message to
   *     show in the toast.
   */
  private onDismissModule_(e: DismissModuleEvent) {
    const id = (e.target as ModuleWrapperElement).module.descriptor.id;
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
    this.$.removeModuleToast.show();
    // Notify the backend.
    NewTabPageProxy.getInstance().handler.onDismissModule(id);
  }

  /**
   * @param e Event notifying a module was disabled. Contains the message to
   *     show in the toast.
   */
  private onDisableModule_(e: DisableModuleEvent) {
    const id = (e.target as ModuleWrapperElement).module.descriptor.id;
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
    this.$.removeModuleToast.show();
    chrome.metricsPrivate.recordSparseHashable(
        'NewTabPage.Modules.Disabled', id);
    chrome.metricsPrivate.recordSparseHashable(
        'NewTabPage.Modules.Disabled.ModuleRequest', id);
  }

  private moduleDisabled_(id: string): boolean {
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
    this.$.removeModuleToast.hide();

    this.removedModuleData_ = null;
  }

  /**
   * Hides and reveals modules depending on removed status.
   * @private
   */
  onRemovedModulesChange_() {
    this.shadowRoot!.querySelectorAll('ntp-module-wrapper')
        .forEach(moduleWrapper => {
          moduleWrapper.parentElement!.hidden =
              this.moduleDisabled_(moduleWrapper.module.descriptor.id);
        });
  }

  /**
   * Module is dragged by updating the module position based on the
   * position of the pointer.
   */
  private onDragStart_(e: MouseEvent) {
    assert(loadTimeData.getBoolean('modulesDragAndDropEnabled'));

    const dragElement = e.target as HTMLElement;
    const dragElementRect = dragElement.getBoundingClientRect();
    // This is the offset between the pointer and module so that the
    // module isn't dragged by the top-left corner.
    const dragOffset = {
      x: e.x - dragElementRect.x,
      y: e.y - dragElementRect.y,
    };

    dragElement.parentElement!.style.width = `${dragElementRect.width}px`;
    dragElement.parentElement!.style.height = `${dragElementRect.height}px`;

    const undraggedModuleWrappers =
        [...this.shadowRoot!.querySelectorAll('ntp-module-wrapper')].filter(
            moduleWrapper => moduleWrapper !== dragElement);

    const dragOver = (e: MouseEvent) => {
      e.preventDefault();

      dragElement.setAttribute('dragging', '');
      dragElement.style.left = `${e.x - dragOffset.x}px`;
      dragElement.style.top = `${e.y - dragOffset.y}px`;
    };

    const dragEnter = (e: MouseEvent) => {
      const moduleContainers = [
        ...this.shadowRoot!.querySelectorAll<HTMLElement>('.module-container')
      ];
      const dragIndex = moduleContainers.indexOf(dragElement.parentElement!);
      const dropIndex =
          moduleContainers.indexOf((e.target as HTMLElement).parentElement!);

      const positionType = dragIndex > dropIndex ? 'beforebegin' : 'afterend';
      const dragContainer = moduleContainers[dragIndex];
      const dropContainer = moduleContainers[dropIndex];

      // To animate the modules as they are reordered we use the FLIP
      // (First, Last, Invert, Play) animation approach by @paullewis.
      // https://aerotwist.com/blog/flip-your-animations/

      // The first and last positions of the modules are used to
      // calculate how the modules have changed.
      const firstRects = undraggedModuleWrappers.map(moduleWrapper => {
        return moduleWrapper.getBoundingClientRect();
      });

      moduleContainers.splice(dragIndex, 1);
      moduleContainers.splice(dropIndex, 0, dragContainer);
      this.appendModuleContainers_(moduleContainers);

      undraggedModuleWrappers.forEach((moduleWrapper, i) => {
        const lastRect = moduleWrapper.getBoundingClientRect();
        const invertX = firstRects[i].left - lastRect.left;
        const invertY = firstRects[i].top - lastRect.top;
        moduleWrapper.animate(
            [
              // A translation is applied to invert the module and make it
              // appear to be in its first position when it actually is in its
              // final position already.
              {transform: `translate(${invertX}px, ${invertY}px)`, zIndex: 0},
              // Removing the inversion translation animates the module from
              // the fake first position to its current (final) position.
              {transform: 'translate(0)', zIndex: 0},
            ],
            {duration: 800, easing: 'ease'});
      });
    };

    undraggedModuleWrappers.forEach(moduleWrapper => {
      moduleWrapper.addEventListener('mouseover', dragEnter);
    });

    this.ownerDocument.addEventListener('mousemove', dragOver);
    this.ownerDocument.addEventListener('mouseup', () => {
      this.ownerDocument.removeEventListener('mousemove', dragOver);

      undraggedModuleWrappers.forEach(moduleWrapper => {
        moduleWrapper.removeEventListener('mouseover', dragEnter);
      });

      // The FLIP approach is also used for the dropping animation
      // of the dragged module because of the position changes caused
      // by removing the dragging styles. (Removing the styles after
      // the animation causes the animation to be fixed.)
      const firstRect = dragElement.getBoundingClientRect();

      dragElement.removeAttribute('dragging');
      dragElement.style.removeProperty('left');
      dragElement.style.removeProperty('top');

      const lastRect = dragElement.getBoundingClientRect();
      const invertX = firstRect.left - lastRect.left;
      const invertY = firstRect.top - lastRect.top;

      dragElement.animate(
          [
            {transform: `translate(${invertX}px, ${invertY}px)`, zIndex: 2},
            {transform: 'translate(0)', zIndex: 2},
          ],
          {duration: 800, easing: 'ease'});

      const moduleIds =
          [...this.shadowRoot!.querySelectorAll('ntp-module-wrapper')].map(
              moduleWrapper => moduleWrapper.module.descriptor.id);
      NewTabPageProxy.getInstance().handler.setModulesOrder(moduleIds);
    }, {once: true});
  }
}

customElements.define(ModulesElement.is, ModulesElement);
