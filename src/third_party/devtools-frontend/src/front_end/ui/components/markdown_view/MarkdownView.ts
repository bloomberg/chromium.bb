// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as LitHtml from '../../lit-html/lit-html.js';
import type * as Marked from '../../../third_party/marked/marked.js';
import * as ComponentHelpers from '../../components/helpers/helpers.js';

import type {MarkdownImageData} from './MarkdownImage.js';
import type {MarkdownLinkData} from './MarkdownLink.js';
import {MarkdownLink} from './MarkdownLink.js';
import {MarkdownImage} from './MarkdownImage.js';

const html = LitHtml.html;
const render = LitHtml.render;

export interface MarkdownViewData {
  tokens: Marked.Marked.Token[];
}

export class MarkdownView extends HTMLElement {
  private readonly shadow = this.attachShadow({mode: 'open'});

  // TODO(crbug.com/1108699): Replace with `Marked.Marked.Token[]` once AST types are fixed upstream.
  private tokenData: readonly Object[] = [];

  set data(data: MarkdownViewData) {
    this.tokenData = data.tokens;
    this.update();
  }

  private update(): void {
    this.render();
  }

  private render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    render(html`
      <style>
      .message {
        line-height: 20px;
        font-size: 14px;
        color: var(--color-text-secondary);
        margin-bottom: 4px;
        user-select: text;
      }

      .message p {
        margin-bottom: 16px;
        margin-block-start: 2px;
      }

      .message ul {
        list-style-type: none;
        list-style-position: inside;
        padding-inline-start: 0;
      }

      .message li {
        margin-top: 8px;
        display: list-item;
      }

      .message li::before {
        content: "→";
        -webkit-mask-image: none;
        padding-right: 5px;
        position: relative;
        top: -1px;
      }

      .message code {
        color: var(--color-text-primary);
        font-size: 12px;
        user-select: text;
        cursor: text;
        background: var(--color-background-elevation-1);
      }
      </style>
      <div class='message'>
        ${this.tokenData.map(renderToken)}
      </div>
    `, this.shadow);
    // clang-format on
  }
}

ComponentHelpers.CustomElements.defineComponent('devtools-markdown-view', MarkdownView);

declare global {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  interface HTMLElementTagNameMap {
    'devtools-markdown-view': MarkdownView;
  }
}

// TODO(crbug.com/1108699): Fix types when they are available.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const renderChildTokens = (token: any): string => {
  return token.tokens.map(renderToken);
};

const unescape = (text: string): string => {
  // Unescape will get rid of the escaping done by Marked to avoid double escaping due to escaping it also with Lit-html
  // Table taken from: front_end/third_party/marked/package/src/helpers.js
  /** @type {Map<string,string>} */
  const escapeReplacements = new Map<string, string>([
    ['&amp;', '&'],
    ['&lt;', '<'],
    ['&gt;', '>'],
    ['&quot;', '"'],
    ['&#39;', '\''],
  ]);
  return text.replace(/&(amp|lt|gt|quot|#39);/g, (matchedString: string) => {
    const replacement = escapeReplacements.get(matchedString);
    return replacement ? replacement : matchedString;
  });
};
// TODO(crbug.com/1108699): Fix types when they are available.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const renderText = (token: any): LitHtml.TemplateResult => {
  if (token.tokens && token.tokens.length > 0) {
    return html`${renderChildTokens(token)}`;
  }
  // Due to unescaping, unescaped html entities (see escapeReplacements' keys) will be rendered
  // as their corresponding symbol while the rest will be rendered as verbatim.
  // Marked's escape function can be found in front_end/third_party/marked/package/src/helpers.js
  return html`${unescape(token.text)}`;
};

// TODO(crbug.com/1108699): Fix types when they are available.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const tokenRenderers = new Map<string, (token: any) => LitHtml.TemplateResult>([
  ['paragraph', (token): LitHtml.TemplateResult => html`<p>${renderChildTokens(token)}</p>`],
  ['list', (token): LitHtml.TemplateResult => html`<ul>${token.items.map(renderToken)}</ul>`],
  ['list_item', (token): LitHtml.TemplateResult => html`<li>${renderChildTokens(token)}</li>`],
  ['text', renderText],
  ['codespan', (token): LitHtml.TemplateResult => html`<code>${unescape(token.text)}</code>`],
  ['space', (): LitHtml.TemplateResult => html``],
  [
    'link',
    (token): LitHtml.TemplateResult => html`<${MarkdownLink.litTagName} .data=${
        {key: token.href, title: token.text} as MarkdownLinkData}></${MarkdownLink.litTagName}>`,
  ],
  [
    'image',
    (token): LitHtml.TemplateResult => html`<${MarkdownImage.litTagName} .data=${
        {key: token.href, title: token.text} as MarkdownImageData}></${MarkdownImage.litTagName}>`,
  ],
]);

// TODO(crbug.com/1108699): Fix types when they are available.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export const renderToken = (token: any): LitHtml.TemplateResult => {
  const renderFn = tokenRenderers.get(token.type);
  if (!renderFn) {
    throw new Error(`Markdown token type '${token.type}' not supported.`);
  }
  return renderFn(token);
};
