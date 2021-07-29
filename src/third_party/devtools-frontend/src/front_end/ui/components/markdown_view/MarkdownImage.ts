// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ComponentHelpers from '../../components/helpers/helpers.js';
import * as IconButton from '../../components/icon_button/icon_button.js';
import * as LitHtml from '../../lit-html/lit-html.js';
import markdownImageStyles from './markdownImage.css.js';

import type {ImageData} from './MarkdownImagesMap.js';
import {getMarkdownImage} from './MarkdownImagesMap.js';

export interface MarkdownImageData {
  key: string;
  title: string;
}

/**
 * Component to render images from parsed markdown.
 * Parsed images from markdown are not directly rendered, instead they have to be added to the MarkdownImagesMap.ts.
 * This makes sure that all icons/images are accounted for in markdown.
 */
export class MarkdownImage extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-markdown-image`;

  private readonly shadow = this.attachShadow({mode: 'open'});
  private imageData?: ImageData;
  private imageTitle?: string;

  constructor() {
    super();
  }

  connectedCallback(): void {
    this.shadow.adoptedStyleSheets = [markdownImageStyles];
  }

  set data(data: MarkdownImageData) {
    const {key, title} = data;
    const markdownImage = getMarkdownImage(key);
    this.imageData = markdownImage;
    this.imageTitle = title;
    this.render();
  }

  private getIconComponent(): LitHtml.TemplateResult {
    if (!this.imageData) {
      return LitHtml.html``;
    }
    const {src, color, width = '100%', height = '100%'} = this.imageData;
    return LitHtml.html`
      <${IconButton.Icon.Icon.litTagName} .data=${
        {iconPath: src, color, width, height} as IconButton.Icon.IconData}></${IconButton.Icon.Icon.litTagName}>
    `;
  }

  private getImageComponent(): LitHtml.TemplateResult {
    if (!this.imageData) {
      return LitHtml.html``;
    }
    const {src, width = '100%', height = '100%'} = this.imageData;
    return LitHtml.html`
      <img class="markdown-image" src=${src} alt=${this.imageTitle} width=${width} height=${height}/>
    `;
  }

  private render(): void {
    if (!this.imageData) {
      return;
    }
    const {isIcon} = this.imageData;
    const imageComponent = isIcon ? this.getIconComponent() : this.getImageComponent();
    LitHtml.render(imageComponent, this.shadow);
  }
}

ComponentHelpers.CustomElements.defineComponent('devtools-markdown-image', MarkdownImage);

declare global {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  interface HTMLElementTagNameMap {
    'devtools-markdown-image': MarkdownImage;
  }
}
