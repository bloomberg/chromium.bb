// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Common from '../../core/common/common.js';

import type {ContentProvider, DeferredContent, SearchMatch} from './ContentProvider.js';
import {performSearchInContent} from './TextUtils.js';

export class StaticContentProvider implements ContentProvider {
  private readonly contentURLInternal: string;
  private readonly contentTypeInternal: Common.ResourceType.ResourceType;
  private readonly lazyContent: () => Promise<DeferredContent>;

  constructor(
      contentURL: string, contentType: Common.ResourceType.ResourceType, lazyContent: () => Promise<DeferredContent>) {
    this.contentURLInternal = contentURL;
    this.contentTypeInternal = contentType;
    this.lazyContent = lazyContent;
  }

  static fromString(contentURL: string, contentType: Common.ResourceType.ResourceType, content: string):
      StaticContentProvider {
    const lazyContent = (): Promise<{
      content: string,
      isEncoded: boolean,
    }> => Promise.resolve({content, isEncoded: false});
    return new StaticContentProvider(contentURL, contentType, lazyContent);
  }

  contentURL(): string {
    return this.contentURLInternal;
  }

  contentType(): Common.ResourceType.ResourceType {
    return this.contentTypeInternal;
  }

  contentEncoded(): Promise<boolean> {
    return Promise.resolve(false);
  }

  requestContent(): Promise<DeferredContent> {
    return this.lazyContent();
  }

  async searchInContent(query: string, caseSensitive: boolean, isRegex: boolean): Promise<SearchMatch[]> {
    const {content} = (await this.lazyContent() as {
      content: string,
      isEncoded: boolean,
    });
    return content ? performSearchInContent(content, query, caseSensitive, isRegex) : [];
  }
}
