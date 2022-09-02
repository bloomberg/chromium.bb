// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../core/common/common.js';
import * as i18n from '../../../core/i18n/i18n.js';
import * as UI from '../../legacy/legacy.js';

import type {AddressChangedEvent, MemoryRequestEvent, Settings, SettingsChangedEvent} from './LinearMemoryInspector.js';
import {LinearMemoryInspector} from './LinearMemoryInspector.js';
import type {LazyUint8Array} from './LinearMemoryInspectorController.js';
import {LinearMemoryInspectorController} from './LinearMemoryInspectorController.js';

const UIStrings = {
  /**
  *@description Label in the Linear Memory Inspector tool that serves as a placeholder if no inspections are open (i.e. nothing to see here).
  *             Inspection hereby refers to viewing, navigating and understanding the memory through this tool.
  */
  noOpenInspections: 'No open inspections',
};
const str_ =
    i18n.i18n.registerUIStrings('ui/components/linear_memory_inspector/LinearMemoryInspectorPane.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
let inspectorInstance: LinearMemoryInspectorPaneImpl;

let wrapperInstance: Wrapper;

export class Wrapper extends UI.Widget.VBox {
  view: LinearMemoryInspectorPaneImpl;
  private constructor() {
    super();
    this.view = LinearMemoryInspectorPaneImpl.instance();
  }

  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): Wrapper {
    const {forceNew} = opts;
    if (!wrapperInstance || forceNew) {
      wrapperInstance = new Wrapper();
    }

    return wrapperInstance;
  }

  wasShown(): void {
    this.view.show(this.contentElement);
  }
}

export class LinearMemoryInspectorPaneImpl extends Common.ObjectWrapper.eventMixin<EventTypes, typeof UI.Widget.VBox>(
    UI.Widget.VBox) {
  readonly #tabbedPane: UI.TabbedPane.TabbedPane;
  readonly #tabIdToInspectorView: Map<string, LinearMemoryInspectorView>;
  constructor() {
    super(false);
    const placeholder = document.createElement('div');
    placeholder.textContent = i18nString(UIStrings.noOpenInspections);
    placeholder.style.display = 'flex';
    this.#tabbedPane = new UI.TabbedPane.TabbedPane();
    this.#tabbedPane.setPlaceholderElement(placeholder);
    this.#tabbedPane.setCloseableTabs(true);
    this.#tabbedPane.setAllowTabReorder(true, true);
    this.#tabbedPane.addEventListener(UI.TabbedPane.Events.TabClosed, this.#tabClosed, this);
    this.#tabbedPane.show(this.contentElement);

    this.#tabIdToInspectorView = new Map();
  }

  static instance(): LinearMemoryInspectorPaneImpl {
    if (!inspectorInstance) {
      inspectorInstance = new LinearMemoryInspectorPaneImpl();
    }
    return inspectorInstance;
  }

  create(tabId: string, title: string, arrayWrapper: LazyUint8Array, address?: number): void {
    const inspectorView = new LinearMemoryInspectorView(arrayWrapper, address);
    this.#tabIdToInspectorView.set(tabId, inspectorView);
    this.#tabbedPane.appendTab(tabId, title, inspectorView, undefined, false, true);
    this.#tabbedPane.selectTab(tabId);
  }

  close(tabId: string): void {
    this.#tabbedPane.closeTab(tabId, false);
  }

  reveal(tabId: string, address?: number): void {
    const view = this.#tabIdToInspectorView.get(tabId);
    if (!view) {
      throw new Error(`No linear memory inspector view for given tab id: ${tabId}`);
    }

    if (address !== undefined) {
      view.updateAddress(address);
    }
    this.refreshView(tabId);
    this.#tabbedPane.selectTab(tabId);
  }

  refreshView(tabId: string): void {
    const view = this.#tabIdToInspectorView.get(tabId);
    if (!view) {
      throw new Error(`View for specified tab id does not exist: ${tabId}`);
    }
    view.refreshData();
  }

  #tabClosed(event: Common.EventTarget.EventTargetEvent<UI.TabbedPane.EventData>): void {
    const {tabId} = event.data;
    this.#tabIdToInspectorView.delete(tabId);
    this.dispatchEventToListeners(Events.ViewClosed, tabId);
  }
}

export const enum Events {
  ViewClosed = 'ViewClosed',
}

export type EventTypes = {
  [Events.ViewClosed]: string,
};

class LinearMemoryInspectorView extends UI.Widget.VBox {
  #memoryWrapper: LazyUint8Array;
  #address: number;
  #inspector: LinearMemoryInspector;
  firstTimeOpen: boolean;
  constructor(memoryWrapper: LazyUint8Array, address: number|undefined = 0) {
    super(false);

    if (address < 0 || address >= memoryWrapper.length()) {
      throw new Error('Requested address is out of bounds.');
    }

    this.#memoryWrapper = memoryWrapper;
    this.#address = address;
    this.#inspector = new LinearMemoryInspector();
    this.#inspector.addEventListener('memoryrequest', (event: MemoryRequestEvent) => {
      this.#memoryRequested(event);
    });
    this.#inspector.addEventListener('addresschanged', (event: AddressChangedEvent) => {
      this.updateAddress(event.data);
    });
    this.#inspector.addEventListener('settingschanged', (event: SettingsChangedEvent) => {
      // Stop event from bubbling up, since no element further up needs the event.
      event.stopPropagation();
      this.saveSettings(event.data);
    });
    this.contentElement.appendChild(this.#inspector);
    this.firstTimeOpen = true;
  }

  wasShown(): void {
    this.refreshData();
  }

  saveSettings(settings: Settings): void {
    LinearMemoryInspectorController.instance().saveSettings(settings);
  }

  updateAddress(address: number): void {
    if (address < 0 || address >= this.#memoryWrapper.length()) {
      throw new Error('Requested address is out of bounds.');
    }
    this.#address = address;
  }

  refreshData(): void {
    void LinearMemoryInspectorController.getMemoryForAddress(this.#memoryWrapper, this.#address).then(({
                                                                                                        memory,
                                                                                                        offset,
                                                                                                      }) => {
      let valueTypes;
      let valueTypeModes;
      let endianness;
      if (this.firstTimeOpen) {
        const settings = LinearMemoryInspectorController.instance().loadSettings();
        valueTypes = settings.valueTypes;
        valueTypeModes = settings.modes;
        endianness = settings.endianness;
        this.firstTimeOpen = false;
      }
      this.#inspector.data = {
        memory,
        address: this.#address,
        memoryOffset: offset,
        outerMemoryLength: this.#memoryWrapper.length(),
        valueTypes,
        valueTypeModes,
        endianness,
      };
    });
  }

  #memoryRequested(event: MemoryRequestEvent): void {
    const {start, end, address} = event.data;
    if (address < start || address >= end) {
      throw new Error('Requested address is out of bounds.');
    }

    void LinearMemoryInspectorController.getMemoryRange(this.#memoryWrapper, start, end).then(memory => {
      this.#inspector.data = {
        memory: memory,
        address: address,
        memoryOffset: start,
        outerMemoryLength: this.#memoryWrapper.length(),
      };
    });
  }
}
