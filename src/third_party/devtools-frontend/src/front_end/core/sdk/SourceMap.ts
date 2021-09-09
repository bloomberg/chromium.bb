/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* eslint-disable rulesdir/no_underscored_properties */

import * as TextUtils from '../../models/text_utils/text_utils.js';
import * as Common from '../common/common.js';
import * as i18n from '../i18n/i18n.js';
import * as Platform from '../platform/platform.js';

import {CompilerSourceMappingContentProvider} from './CompilerSourceMappingContentProvider.js';
import type {PageResourceLoadInitiator} from './PageResourceLoader.js';
import {PageResourceLoader} from './PageResourceLoader.js';  // eslint-disable-line no-unused-vars

const UIStrings = {
  /**
  *@description Error message when failing to load a source map text via the network
  *@example {https://example.com/sourcemap.map} PH1
  *@example {A certificate error occurred} PH2
  */
  couldNotLoadContentForSS: 'Could not load content for {PH1}: {PH2}',
  /**
  *@description Error message when failing to load a script source text via the network
  *@example {https://example.com} PH1
  *@example {Unexpected token} PH2
  */
  couldNotParseContentForSS: 'Could not parse content for {PH1}: {PH2}',
};
const str_ = i18n.i18n.registerUIStrings('core/sdk/SourceMap.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export interface SourceMap {
  compiledURL(): string;
  url(): string;
  sourceURLs(): string[];
  sourceContentProvider(sourceURL: string, contentType: Common.ResourceType.ResourceType):
      TextUtils.ContentProvider.ContentProvider;
  embeddedContentByURL(sourceURL: string): string|null;
  findEntry(lineNumber: number, columnNumber: number): SourceMapEntry|null;
  sourceLineMapping(sourceURL: string, lineNumber: number, columnNumber: number): SourceMapEntry|null;
  mappings(): SourceMapEntry[];
  mapsOrigin(): boolean;
}

class SourceMapV3 {
  version!: number;
  file!: string|undefined;
  sources!: string[];
  sections!: Section[]|undefined;
  mappings!: string;
  sourceRoot!: string|undefined;
  names!: string[]|undefined;
  sourcesContent!: string|undefined;
  constructor() {
  }
}

class Section {
  map!: SourceMapV3;
  offset!: Offset;
  url!: string|undefined;
  constructor() {
  }
}

class Offset {
  line!: number;
  column!: number;
  constructor() {
  }
}

export class SourceMapEntry {
  lineNumber: number;
  columnNumber: number;
  sourceURL: string|undefined;
  sourceLineNumber: number;
  sourceColumnNumber: number;
  name: string|undefined;

  constructor(
      lineNumber: number, columnNumber: number, sourceURL?: string, sourceLineNumber?: number,
      sourceColumnNumber?: number, name?: string) {
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber;
    this.sourceURL = sourceURL;
    this.sourceLineNumber = (sourceLineNumber as number);
    this.sourceColumnNumber = (sourceColumnNumber as number);
    this.name = name;
  }

  static compare(entry1: SourceMapEntry, entry2: SourceMapEntry): number {
    if (entry1.lineNumber !== entry2.lineNumber) {
      return entry1.lineNumber - entry2.lineNumber;
    }
    return entry1.columnNumber - entry2.columnNumber;
  }
}

export class EditResult {
  map: SourceMap;
  compiledEdits: TextUtils.TextRange.SourceEdit[];
  newSources: Map<string, string>;
  constructor(map: SourceMap, compiledEdits: TextUtils.TextRange.SourceEdit[], newSources: Map<string, string>) {
    this.map = map;
    this.compiledEdits = compiledEdits;
    this.newSources = newSources;
  }
}

const base64Digits = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const base64Map = new Map<string, number>();

for (let i = 0; i < base64Digits.length; ++i) {
  base64Map.set(base64Digits.charAt(i), i);
}

const sourceMapToSourceList = new WeakMap<SourceMapV3, string[]>();

export class TextSourceMap implements SourceMap {
  _initiator: PageResourceLoadInitiator;
  _json: SourceMapV3|null;
  _compiledURL: string;
  _sourceMappingURL: string;
  _baseURL: string;
  _mappings: SourceMapEntry[]|null;
  _sourceInfos: Map<string, TextSourceMap.SourceInfo>;

  /**
   * Implements Source Map V3 model. See https://github.com/google/closure-compiler/wiki/Source-Maps
   * for format description.
   */
  constructor(
      compiledURL: string, sourceMappingURL: string, payload: SourceMapV3, initiator: PageResourceLoadInitiator) {
    this._initiator = initiator;
    this._json = payload;
    this._compiledURL = compiledURL;
    this._sourceMappingURL = sourceMappingURL;
    this._baseURL = sourceMappingURL.startsWith('data:') ? compiledURL : sourceMappingURL;

    this._mappings = null;
    this._sourceInfos = new Map();
    if (this._json.sections) {
      const sectionWithURL = Boolean(this._json.sections.find(section => Boolean(section.url)));
      if (sectionWithURL) {
        Common.Console.Console.instance().warn(
            `SourceMap "${sourceMappingURL}" contains unsupported "URL" field in one of its sections.`);
      }
    }
    this._eachSection(this._parseSources.bind(this));
  }

  /**
   * @throws {!Error}
   */
  static async load(sourceMapURL: string, compiledURL: string, initiator: PageResourceLoadInitiator):
      Promise<TextSourceMap> {
    let updatedContent;
    try {
      const {content} = await PageResourceLoader.instance().loadResource(sourceMapURL, initiator);
      updatedContent = content;
      if (content.slice(0, 3) === ')]}') {
        updatedContent = content.substring(content.indexOf('\n'));
      }
    } catch (error) {
      throw new Error(i18nString(UIStrings.couldNotLoadContentForSS, {PH1: sourceMapURL, PH2: error.message}));
    }

    try {
      const payload = (JSON.parse(updatedContent) as SourceMapV3);
      return new TextSourceMap(compiledURL, sourceMapURL, payload, initiator);
    } catch (error) {
      throw new Error(i18nString(UIStrings.couldNotParseContentForSS, {PH1: sourceMapURL, PH2: error.message}));
    }
  }

  compiledURL(): string {
    return this._compiledURL;
  }

  url(): string {
    return this._sourceMappingURL;
  }

  sourceURLs(): string[] {
    return [...this._sourceInfos.keys()];
  }

  sourceContentProvider(sourceURL: string, contentType: Common.ResourceType.ResourceType):
      TextUtils.ContentProvider.ContentProvider {
    const info = this._sourceInfos.get(sourceURL);
    if (info && info.content) {
      return TextUtils.StaticContentProvider.StaticContentProvider.fromString(sourceURL, contentType, info.content);
    }
    return new CompilerSourceMappingContentProvider(sourceURL, contentType, this._initiator);
  }

  embeddedContentByURL(sourceURL: string): string|null {
    const entry = this._sourceInfos.get(sourceURL);
    if (!entry) {
      return null;
    }
    return entry.content;
  }

  findEntry(lineNumber: number, columnNumber: number): SourceMapEntry|null {
    const mappings = this.mappings();
    const index = Platform.ArrayUtilities.upperBound(
        mappings, undefined, (unused, entry) => lineNumber - entry.lineNumber || columnNumber - entry.columnNumber);
    return index ? mappings[index - 1] : null;
  }

  sourceLineMapping(sourceURL: string, lineNumber: number, columnNumber: number): SourceMapEntry|null {
    const mappings = this._reversedMappings(sourceURL);
    const first = Platform.ArrayUtilities.lowerBound(mappings, lineNumber, lineComparator);
    const last = Platform.ArrayUtilities.upperBound(mappings, lineNumber, lineComparator);
    if (first >= mappings.length || mappings[first].sourceLineNumber !== lineNumber) {
      return null;
    }
    const columnMappings = mappings.slice(first, last);
    if (!columnMappings.length) {
      return null;
    }
    const index = Platform.ArrayUtilities.lowerBound(
        columnMappings, columnNumber, (columnNumber, mapping) => columnNumber - mapping.sourceColumnNumber);
    return index >= columnMappings.length ? columnMappings[columnMappings.length - 1] : columnMappings[index];

    function lineComparator(lineNumber: number, mapping: SourceMapEntry): number {
      return lineNumber - mapping.sourceLineNumber;
    }
  }

  findReverseEntries(sourceURL: string, lineNumber: number, columnNumber: number): SourceMapEntry[] {
    const mappings = this._reversedMappings(sourceURL);
    const endIndex = Platform.ArrayUtilities.upperBound(
        mappings, undefined,
        (unused, entry) => lineNumber - entry.sourceLineNumber || columnNumber - entry.sourceColumnNumber);
    let startIndex = endIndex;
    while (startIndex > 0 && mappings[startIndex - 1].sourceLineNumber === mappings[endIndex - 1].sourceLineNumber &&
           mappings[startIndex - 1].sourceColumnNumber === mappings[endIndex - 1].sourceColumnNumber) {
      --startIndex;
    }

    return mappings.slice(startIndex, endIndex);
  }

  mappings(): SourceMapEntry[] {
    if (this._mappings === null) {
      this._mappings = [];
      this._eachSection(this._parseMap.bind(this));
      this._json = null;
    }
    return /** @type {!Array<!SourceMapEntry>} */ this._mappings as SourceMapEntry[];
  }

  _reversedMappings(sourceURL: string): SourceMapEntry[] {
    const info = this._sourceInfos.get(sourceURL);
    if (!info) {
      return [];
    }
    const mappings = this.mappings();
    if (info.reverseMappings === null) {
      info.reverseMappings = mappings.filter(mapping => mapping.sourceURL === sourceURL).sort(sourceMappingComparator);
    }

    return info.reverseMappings;

    function sourceMappingComparator(a: SourceMapEntry, b: SourceMapEntry): number {
      if (a.sourceLineNumber !== b.sourceLineNumber) {
        return a.sourceLineNumber - b.sourceLineNumber;
      }
      if (a.sourceColumnNumber !== b.sourceColumnNumber) {
        return a.sourceColumnNumber - b.sourceColumnNumber;
      }

      if (a.lineNumber !== b.lineNumber) {
        return a.lineNumber - b.lineNumber;
      }

      return a.columnNumber - b.columnNumber;
    }
  }

  _eachSection(callback: (arg0: SourceMapV3, arg1: number, arg2: number) => void): void {
    if (!this._json) {
      return;
    }
    if (!this._json.sections) {
      callback(this._json, 0, 0);
      return;
    }
    for (const section of this._json.sections) {
      callback(section.map, section.offset.line, section.offset.column);
    }
  }

  _parseSources(sourceMap: SourceMapV3): void {
    const sourcesList = [];
    let sourceRoot = sourceMap.sourceRoot || '';
    if (sourceRoot && !sourceRoot.endsWith('/')) {
      sourceRoot += '/';
    }
    for (let i = 0; i < sourceMap.sources.length; ++i) {
      const href = sourceRoot + sourceMap.sources[i];
      let url = Common.ParsedURL.ParsedURL.completeURL(this._baseURL, href) || href;
      const source = sourceMap.sourcesContent && sourceMap.sourcesContent[i];
      if (url === this._compiledURL && source) {
        url += '? [sm]';
      }
      this._sourceInfos.set(url, new TextSourceMap.SourceInfo(source || null, null));
      sourcesList.push(url);
    }
    sourceMapToSourceList.set(sourceMap, sourcesList);
  }

  _parseMap(map: SourceMapV3, lineNumber: number, columnNumber: number): void {
    let sourceIndex = 0;
    let sourceLineNumber = 0;
    let sourceColumnNumber = 0;
    let nameIndex = 0;
    // TODO(crbug.com/1011811): refactor away map.
    // `sources` can be undefined if it wasn't previously
    // processed and added to the list. However, that
    // is not WAI and we should make sure that we can
    // only reach this point when we are certain
    // we have the list available.
    const sources = sourceMapToSourceList.get(map);
    const names = map.names || [];
    const stringCharIterator = new TextSourceMap.StringCharIterator(map.mappings);
    let sourceURL: string|(string | undefined) = sources && sources[sourceIndex];

    while (true) {
      if (stringCharIterator.peek() === ',') {
        stringCharIterator.next();
      } else {
        while (stringCharIterator.peek() === ';') {
          lineNumber += 1;
          columnNumber = 0;
          stringCharIterator.next();
        }
        if (!stringCharIterator.hasNext()) {
          break;
        }
      }

      columnNumber += this._decodeVLQ(stringCharIterator);
      if (!stringCharIterator.hasNext() || this._isSeparator(stringCharIterator.peek())) {
        this.mappings().push(new SourceMapEntry(lineNumber, columnNumber));
        continue;
      }

      const sourceIndexDelta = this._decodeVLQ(stringCharIterator);
      if (sourceIndexDelta) {
        sourceIndex += sourceIndexDelta;
        if (sources) {
          sourceURL = sources[sourceIndex];
        }
      }
      sourceLineNumber += this._decodeVLQ(stringCharIterator);
      sourceColumnNumber += this._decodeVLQ(stringCharIterator);

      if (!stringCharIterator.hasNext() || this._isSeparator(stringCharIterator.peek())) {
        this.mappings().push(
            new SourceMapEntry(lineNumber, columnNumber, sourceURL, sourceLineNumber, sourceColumnNumber));
        continue;
      }

      nameIndex += this._decodeVLQ(stringCharIterator);
      this.mappings().push(new SourceMapEntry(
          lineNumber, columnNumber, sourceURL, sourceLineNumber, sourceColumnNumber, names[nameIndex]));
    }

    // As per spec, mappings are not necessarily sorted.
    this.mappings().sort(SourceMapEntry.compare);
  }

  _isSeparator(char: string): boolean {
    return char === ',' || char === ';';
  }

  _decodeVLQ(stringCharIterator: TextSourceMap.StringCharIterator): number {
    // Read unsigned value.
    let result = 0;
    let shift = 0;
    let digit: number = TextSourceMap._VLQ_CONTINUATION_MASK;
    while (digit & TextSourceMap._VLQ_CONTINUATION_MASK) {
      digit = base64Map.get(stringCharIterator.next()) || 0;
      result += (digit & TextSourceMap._VLQ_BASE_MASK) << shift;
      shift += TextSourceMap._VLQ_BASE_SHIFT;
    }

    // Fix the sign.
    const negative = result & 1;
    result >>= 1;
    return negative ? -result : result;
  }

  reverseMapTextRange(url: string, textRange: TextUtils.TextRange.TextRange): TextUtils.TextRange.TextRange|null {
    function comparator(
        position: {
          lineNumber: number,
          columnNumber: number,
        },
        mapping: SourceMapEntry): number {
      if (position.lineNumber !== mapping.sourceLineNumber) {
        return position.lineNumber - mapping.sourceLineNumber;
      }

      return position.columnNumber - mapping.sourceColumnNumber;
    }

    const mappings = this._reversedMappings(url);
    if (!mappings.length) {
      return null;
    }
    const startIndex = Platform.ArrayUtilities.lowerBound(
        mappings, {lineNumber: textRange.startLine, columnNumber: textRange.startColumn}, comparator);
    const endIndex = Platform.ArrayUtilities.upperBound(
        mappings, {lineNumber: textRange.endLine, columnNumber: textRange.endColumn}, comparator);

    const startMapping = mappings[startIndex];
    const endMapping = mappings[endIndex];
    return new TextUtils.TextRange.TextRange(
        startMapping.lineNumber, startMapping.columnNumber, endMapping.lineNumber, endMapping.columnNumber);
  }

  mapsOrigin(): boolean {
    const mappings = this.mappings();
    if (mappings.length > 0) {
      const firstEntry = mappings[0];
      return firstEntry?.lineNumber === 0 || firstEntry.columnNumber === 0;
    }
    return false;
  }
}

export namespace TextSourceMap {
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  export const _VLQ_BASE_SHIFT = 5;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  export const _VLQ_BASE_MASK = (1 << 5) - 1;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  export const _VLQ_CONTINUATION_MASK = 1 << 5;

  export class StringCharIterator {
    _string: string;
    _position: number;

    constructor(string: string) {
      this._string = string;
      this._position = 0;
    }

    next(): string {
      return this._string.charAt(this._position++);
    }

    peek(): string {
      return this._string.charAt(this._position);
    }

    hasNext(): boolean {
      return this._position < this._string.length;
    }
  }

  export class SourceInfo {
    content: string|null;
    reverseMappings: SourceMapEntry[]|null;

    constructor(content: string|null, reverseMappings: SourceMapEntry[]|null) {
      this.content = content;
      this.reverseMappings = reverseMappings;
    }
  }
}
