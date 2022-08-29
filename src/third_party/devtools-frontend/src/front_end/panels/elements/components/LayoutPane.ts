// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../core/common/common.js';
import * as ComponentHelpers from '../../../ui/components/helpers/helpers.js';
import * as UI from '../../../ui/legacy/legacy.js';
import * as LitHtml from '../../../ui/lit-html/lit-html.js';

import type {BooleanSetting, EnumSetting, Setting} from './LayoutPaneUtils.js';
import type {LayoutElement} from './LayoutPaneUtils.js';

import type {NodeTextData} from './NodeText.js';
import {NodeText} from './NodeText.js';
import layoutPaneStyles from '../layoutPane.css.js';
import * as Input from '../../../ui/components/input/input.js';
// eslint-disable-next-line rulesdir/es_modules_import
import inspectorCommonStyles from '../../../ui/legacy/inspectorCommon.css.js';

import * as i18n from '../../../core/i18n/i18n.js';
const UIStrings = {
  /**
  *@description Title of the input to select the overlay color for an element using the color picker
  */
  chooseElementOverlayColor: 'Choose the overlay color for this element',
  /**
  *@description Title of the show element button in the Layout pane of the Elements panel
  */
  showElementInTheElementsPanel: 'Show element in the Elements panel',
  /**
  *@description Title of a section on CSS Grid tooling
  */
  grid: 'Grid',
  /**
  *@description Title of a section in the Layout Sidebar pane of the Elements panel
  */
  overlayDisplaySettings: 'Overlay display settings',
  /**
  *@description Title of a section in Layout sidebar pane
  */
  gridOverlays: 'Grid overlays',
  /**
  *@description Message in the Layout panel informing users that no CSS Grid layouts were found on the page
  */
  noGridLayoutsFoundOnThisPage: 'No grid layouts found on this page',
  /**
  *@description Title of the Flexbox section in the Layout panel
  */
  flexbox: 'Flexbox',
  /**
  *@description Title of a section in the Layout panel
  */
  flexboxOverlays: 'Flexbox overlays',
  /**
  *@description Text in the Layout panel, when no flexbox elements are found
  */
  noFlexboxLayoutsFoundOnThisPage: 'No flexbox layouts found on this page',
  /**
  *@description Screen reader announcement when opening color picker tool.
  */
  colorPickerOpened: 'Color picker opened.',
};
const str_ = i18n.i18n.registerUIStrings('panels/elements/components/LayoutPane.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
export {LayoutElement};

const {render, html} = LitHtml;

export class SettingChangedEvent extends Event {
  static readonly eventName = 'settingchanged';
  data: {setting: string, value: string|boolean};

  constructor(setting: string, value: string|boolean) {
    super(SettingChangedEvent.eventName, {});
    this.data = {setting, value};
  }
}

interface HTMLInputElementEvent extends Event {
  target: HTMLInputElement;
}

function isEnumSetting(setting: Setting): setting is EnumSetting {
  return setting.type === Common.Settings.SettingType.ENUM;
}

function isBooleanSetting(setting: Setting): setting is BooleanSetting {
  return setting.type === Common.Settings.SettingType.BOOLEAN;
}

export interface LayoutPaneData {
  settings: Setting[];
  gridElements: LayoutElement[];
  flexContainerElements?: LayoutElement[];
}

export class LayoutPane extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-layout-pane`;
  readonly #shadow = this.attachShadow({mode: 'open'});
  #settings: Readonly<Setting[]> = [];
  #gridElements: Readonly<LayoutElement[]> = [];
  #flexContainerElements?: Readonly<LayoutElement[]> = [];

  constructor() {
    super();
    this.#shadow.adoptedStyleSheets = [
      Input.checkboxStyles,
      layoutPaneStyles,
      inspectorCommonStyles,
    ];
  }

  set data(data: LayoutPaneData) {
    this.#settings = data.settings;
    this.#gridElements = data.gridElements;
    this.#flexContainerElements = data.flexContainerElements;
    this.#render();
  }

  #onSummaryKeyDown(event: KeyboardEvent): void {
    if (!event.target) {
      return;
    }
    const summaryElement = event.target as HTMLElement;
    const detailsElement = summaryElement.parentElement as HTMLDetailsElement;
    if (!detailsElement) {
      throw new Error('<details> element is not found for a <summary> element');
    }
    switch (event.key) {
      case 'ArrowLeft':
        detailsElement.open = false;
        break;
      case 'ArrowRight':
        detailsElement.open = true;
        break;
    }
  }

  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    render(html`
      <details open>
        <summary class="header" @keydown=${this.#onSummaryKeyDown}>
          ${i18nString(UIStrings.grid)}
        </summary>
        <div class="content-section">
          <h3 class="content-section-title">${i18nString(UIStrings.overlayDisplaySettings)}</h3>
          <div class="select-settings">
            ${this.#getEnumSettings().map(setting => this.#renderEnumSetting(setting))}
          </div>
          <div class="checkbox-settings">
            ${this.#getBooleanSettings().map(setting => this.#renderBooleanSetting(setting))}
          </div>
        </div>
        ${this.#gridElements ?
          html`<div class="content-section">
            <h3 class="content-section-title">
              ${this.#gridElements.length ? i18nString(UIStrings.gridOverlays) : i18nString(UIStrings.noGridLayoutsFoundOnThisPage)}
            </h3>
            ${this.#gridElements.length ?
              html`<div class="elements">
                ${this.#gridElements.map(element => this.#renderElement(element))}
              </div>` : ''}
          </div>` : ''}
      </details>
      ${this.#flexContainerElements !== undefined ?
        html`
        <details open>
          <summary class="header" @keydown=${this.#onSummaryKeyDown}>
            ${i18nString(UIStrings.flexbox)}
          </summary>
          ${this.#flexContainerElements ?
            html`<div class="content-section">
              <h3 class="content-section-title">
                ${this.#flexContainerElements.length ? i18nString(UIStrings.flexboxOverlays) : i18nString(UIStrings.noFlexboxLayoutsFoundOnThisPage)}
              </h3>
              ${this.#flexContainerElements.length ?
                html`<div class="elements">
                  ${this.#flexContainerElements.map(element => this.#renderElement(element))}
                </div>` : ''}
            </div>` : ''}
        </details>
        `
      : ''}
    `, this.#shadow, {
      host: this,
    });
    // clang-format on
  }

  #getEnumSettings(): EnumSetting[] {
    return this.#settings.filter(isEnumSetting);
  }

  #getBooleanSettings(): BooleanSetting[] {
    return this.#settings.filter(isBooleanSetting);
  }

  #onBooleanSettingChange(setting: BooleanSetting, event: HTMLInputElementEvent): void {
    event.preventDefault();
    this.dispatchEvent(new SettingChangedEvent(setting.name, event.target.checked));
  }

  #onEnumSettingChange(setting: EnumSetting, event: HTMLInputElementEvent): void {
    event.preventDefault();
    this.dispatchEvent(new SettingChangedEvent(setting.name, event.target.value));
  }

  #onElementToggle(element: LayoutElement, event: HTMLInputElementEvent): void {
    event.preventDefault();
    element.toggle(event.target.checked);
  }

  #onElementClick(element: LayoutElement, event: HTMLInputElementEvent): void {
    event.preventDefault();
    element.reveal();
  }

  #onColorChange(element: LayoutElement, event: HTMLInputElementEvent): void {
    event.preventDefault();
    element.setColor(event.target.value);
    this.#render();
  }

  #onElementMouseEnter(element: LayoutElement, event: HTMLInputElementEvent): void {
    event.preventDefault();
    element.highlight();
  }

  #onElementMouseLeave(element: LayoutElement, event: HTMLInputElementEvent): void {
    event.preventDefault();
    element.hideHighlight();
  }

  #renderElement(element: LayoutElement): LitHtml.TemplateResult {
    const onElementToggle = this.#onElementToggle.bind(this, element);
    const onElementClick = this.#onElementClick.bind(this, element);
    const onColorChange = this.#onColorChange.bind(this, element);
    const onMouseEnter = this.#onElementMouseEnter.bind(this, element);
    const onMouseLeave = this.#onElementMouseLeave.bind(this, element);
    const onColorLabelKeyUp = (event: KeyboardEvent): void => {
      // Handle Enter and Space events to make the color picker accessible.
      if (event.key !== 'Enter' && event.key !== ' ') {
        return;
      }
      const target = event.target as HTMLLabelElement;
      const input = target.querySelector('input') as HTMLInputElement;
      input.click();
      UI.ARIAUtils.alert(i18nString(UIStrings.colorPickerOpened));
      event.preventDefault();
    };
    const onColorLabelKeyDown = (event: KeyboardEvent): void => {
      // Prevent default scrolling when the Space key is pressed.
      if (event.key === ' ') {
        event.preventDefault();
      }
    };
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    return html`<div class="element">
      <label data-element="true" class="checkbox-label">
        <input data-input="true" type="checkbox" .checked=${element.enabled} @change=${onElementToggle} />
        <span class="node-text-container" data-label="true" @mouseenter=${onMouseEnter} @mouseleave=${onMouseLeave}>
          <${NodeText.litTagName} .data=${{
            nodeId: element.domId,
            nodeTitle: element.name,
            nodeClasses: element.domClasses,
          } as NodeTextData}></${NodeText.litTagName}>
        </span>
      </label>
      <label @keyup=${onColorLabelKeyUp} @keydown=${onColorLabelKeyDown} tabindex="0" title=${i18nString(UIStrings.chooseElementOverlayColor)} class="color-picker-label" style="background: ${element.color};">
        <input @change=${onColorChange} @input=${onColorChange} tabindex="-1" class="color-picker" type="color" value=${element.color} />
      </label>
      <button tabindex="0" @click=${onElementClick} title=${i18nString(UIStrings.showElementInTheElementsPanel)} class="show-element"></button>
    </div>`;
    // clang-format on
  }

  #renderBooleanSetting(setting: BooleanSetting): LitHtml.TemplateResult {
    const onBooleanSettingChange = this.#onBooleanSettingChange.bind(this, setting);
    return html`<label data-boolean-setting="true" class="checkbox-label" title=${setting.title}>
      <input data-input="true" type="checkbox" .checked=${setting.value} @change=${onBooleanSettingChange} />
      <span data-label="true">${setting.title}</span>
    </label>`;
  }

  #renderEnumSetting(setting: EnumSetting): LitHtml.TemplateResult {
    const onEnumSettingChange = this.#onEnumSettingChange.bind(this, setting);
    return html`<label data-enum-setting="true" class="select-label" title=${setting.title}>
      <select class="chrome-select" data-input="true" @change=${onEnumSettingChange}>
        ${
        setting.options.map(
            opt => html`<option value=${opt.value} .selected=${setting.value === opt.value}>${opt.title}</option>`)}
      </select>
    </label>`;
  }
}

ComponentHelpers.CustomElements.defineComponent('devtools-layout-pane', LayoutPane);

declare global {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  interface HTMLElementTagNameMap {
    'devtools-layout-pane': LayoutPane;
  }
}
