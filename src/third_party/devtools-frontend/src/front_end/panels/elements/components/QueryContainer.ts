// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from '../../../core/sdk/sdk.js';
import * as ComponentHelpers from '../../../ui/components/helpers/helpers.js';
import * as IconButton from '../../../ui/components/icon_button/icon_button.js';
import * as LitHtml from '../../../ui/lit-html/lit-html.js';

import type {DOMNode} from './Helper.js';
import {NodeText} from './NodeText.js';
import type {NodeTextData} from './NodeText.js';
import queryContainerStyles from './queryContainer.css.js';

const {render, html} = LitHtml;
const {PhysicalAxis, QueryAxis} = SDK.CSSContainerQuery;

export class QueriedSizeRequestedEvent extends Event {
  constructor() {
    super('queriedsizerequested', {});
  }
}

export interface QueryContainerData {
  container: DOMNode;
  queryName?: string;
  onContainerLinkClick: (event: Event) => void;
}

export class QueryContainer extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-query-container`;

  private readonly shadow = this.attachShadow({mode: 'open'});
  private queryName?: string;
  private container?: DOMNode;
  private onContainerLinkClick?: (event: Event) => void;
  private isContainerLinkHovered = false;
  private queriedSizeDetails?: SDK.CSSContainerQuery.ContainerQueriedSizeDetails;

  set data(data: QueryContainerData) {
    this.queryName = data.queryName;
    this.container = data.container;
    this.onContainerLinkClick = data.onContainerLinkClick;
    this.render();
  }

  connectedCallback(): void {
    this.shadow.adoptedStyleSheets = [queryContainerStyles];
  }

  updateContainerQueriedSizeDetails(details: SDK.CSSContainerQuery.ContainerQueriedSizeDetails): void {
    this.queriedSizeDetails = details;
    this.render();
  }

  private async onContainerLinkMouseEnter(): Promise<void> {
    this.container?.highlightNode('container-outline');
    this.isContainerLinkHovered = true;
    this.dispatchEvent(new QueriedSizeRequestedEvent());
  }

  private onContainerLinkMouseLeave(): void {
    this.container?.clearHighlight();
    this.isContainerLinkHovered = false;
    this.render();
  }

  private render(): void {
    if (!this.container) {
      return;
    }

    let idToDisplay, classesToDisplay;
    if (!this.queryName) {
      idToDisplay = this.container.getAttribute('id');
      classesToDisplay = this.container.getAttribute('class')?.split(/\s+/).filter(Boolean);
    }

    const nodeTitle = this.queryName || this.container.nodeNameNicelyCased;

    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    render(html`
      →
      <a href="#"
        draggable=false
        class="container-link"
        @click=${this.onContainerLinkClick}
        @mouseenter=${this.onContainerLinkMouseEnter}
        @mouseleave=${this.onContainerLinkMouseLeave}
      ><${NodeText.litTagName}
          data-node-title=${nodeTitle}
          .data=${{
        nodeTitle,
        nodeId: idToDisplay,
        nodeClasses: classesToDisplay,
      } as NodeTextData}></${NodeText.litTagName}></a>
      ${this.isContainerLinkHovered ? this.renderQueriedSizeDetails() : LitHtml.nothing}
    `, this.shadow, {
      host: this,
    });
    // clang-format on
  }

  private renderQueriedSizeDetails(): LitHtml.TemplateResult|{} {
    if (!this.queriedSizeDetails || this.queriedSizeDetails.queryAxis === QueryAxis.None) {
      return LitHtml.nothing;
    }

    const areBothAxesQueried = this.queriedSizeDetails.queryAxis === QueryAxis.Both;

    const axisIconClasses = LitHtml.Directives.classMap({
      'axis-icon': true,
      'hidden': areBothAxesQueried,
      'vertical': this.queriedSizeDetails.physicalAxis === PhysicalAxis.Vertical,
    });

    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    return html`
      <span class="queried-size-details">
        (${this.queriedSizeDetails.queryAxis}<${IconButton.Icon.Icon.litTagName}
          class=${axisIconClasses} .data=${{
            iconName: 'ic_dimension_single',
            color: 'var(--color-text-primary)',
          } as IconButton.Icon.IconData}></${IconButton.Icon.Icon.litTagName}>)
        ${areBothAxesQueried && this.queriedSizeDetails.width ? 'width:' : LitHtml.nothing}
        ${this.queriedSizeDetails.width || LitHtml.nothing}
        ${areBothAxesQueried && this.queriedSizeDetails.height ? 'height:' : LitHtml.nothing}
        ${this.queriedSizeDetails.height || LitHtml.nothing}
      </span>
    `;
    // clang-format on
  }
}

ComponentHelpers.CustomElements.defineComponent('devtools-query-container', QueryContainer);

declare global {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  interface HTMLElementTagNameMap {
    'devtools-query-container': QueryContainer;
  }
}
