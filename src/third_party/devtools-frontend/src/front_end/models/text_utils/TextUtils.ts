/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

import * as Platform from '../../core/platform/platform.js';

import {SearchMatch} from './ContentProvider.js';
import {Text} from './Text.js';

export const Utils = {
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  get _keyValueFilterRegex(): RegExp {
    return /(?:^|\s)(\-)?([\w\-]+):([^\s]+)/;
  },
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  get _regexFilterRegex(): RegExp {
    return /(?:^|\s)(\-)?\/([^\s]+)\//;
  },
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  get _textFilterRegex(): RegExp {
    return /(?:^|\s)(\-)?([^\s]+)/;
  },
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  get _SpaceCharRegex(): RegExp {
    return /\s/;
  },
  /**
   * @enum {string}
   */
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  get Indent(): {TwoSpaces: '  ', FourSpaces: '    ', EightSpaces: '        ', TabCharacter: '\t'} {
    return {TwoSpaces: '  ', FourSpaces: '    ', EightSpaces: '        ', TabCharacter: '\t'};
  },

  isStopChar: function(char: string): boolean {
    return (char > ' ' && char < '0') || (char > '9' && char < 'A') || (char > 'Z' && char < '_') ||
        (char > '_' && char < 'a') || (char > 'z' && char <= '~');
  },

  isWordChar: function(char: string): boolean {
    return !Utils.isStopChar(char) && !Utils.isSpaceChar(char);
  },

  isSpaceChar: function(char: string): boolean {
    return Utils._SpaceCharRegex.test(char);
  },

  isWord: function(word: string): boolean {
    for (let i = 0; i < word.length; ++i) {
      if (!Utils.isWordChar(word.charAt(i))) {
        return false;
      }
    }
    return true;
  },

  isOpeningBraceChar: function(char: string): boolean {
    return char === '(' || char === '{';
  },

  isClosingBraceChar: function(char: string): boolean {
    return char === ')' || char === '}';
  },

  isBraceChar: function(char: string): boolean {
    return Utils.isOpeningBraceChar(char) || Utils.isClosingBraceChar(char);
  },

  textToWords: function(text: string, isWordChar: (arg0: string) => boolean, wordCallback: (arg0: string) => void):
      void {
        let startWord = -1;
        for (let i = 0; i < text.length; ++i) {
          if (!isWordChar(text.charAt(i))) {
            if (startWord !== -1) {
              wordCallback(text.substring(startWord, i));
            }
            startWord = -1;
          } else if (startWord === -1) {
            startWord = i;
          }
        }
        if (startWord !== -1) {
          wordCallback(text.substring(startWord));
        }
      },

  lineIndent: function(line: string): string {
    let indentation = 0;
    while (indentation < line.length && Utils.isSpaceChar(line.charAt(indentation))) {
      ++indentation;
    }
    return line.substr(0, indentation);
  },

  isUpperCase: function(text: string): boolean {
    return text === text.toUpperCase();
  },

  isLowerCase: function(text: string): boolean {
    return text === text.toLowerCase();
  },

  splitStringByRegexes(text: string, regexes: RegExp[]): {
    value: string,
    position: number,
    regexIndex: number,
    captureGroups: Array<string|undefined>,
  }[] {
    const matches: {
      value: string,
      position: number,
      regexIndex: number,
      captureGroups: (string|undefined)[],
    }[] = [];
    const globalRegexes: RegExp[] = [];
    for (let i = 0; i < regexes.length; i++) {
      const regex = regexes[i];
      if (!regex.global) {
        globalRegexes.push(new RegExp(regex.source, regex.flags ? regex.flags + 'g' : 'g'));
      } else {
        globalRegexes.push(regex);
      }
    }
    doSplit(text, 0, 0);
    return matches;

    function doSplit(text: string, regexIndex: number, startIndex: number): void {
      if (regexIndex >= globalRegexes.length) {
        // Set regexIndex as -1 if text did not match with any regular expression
        matches.push({value: text, position: startIndex, regexIndex: -1, captureGroups: []});
        return;
      }
      const regex = globalRegexes[regexIndex];
      let currentIndex = 0;
      let result;
      regex.lastIndex = 0;
      while ((result = regex.exec(text)) !== null) {
        const stringBeforeMatch = text.substring(currentIndex, result.index);
        if (stringBeforeMatch) {
          doSplit(stringBeforeMatch, regexIndex + 1, startIndex + currentIndex);
        }
        const match = result[0];
        matches.push({
          value: match,
          position: startIndex + result.index,
          regexIndex: regexIndex,
          captureGroups: result.slice(1),
        });
        currentIndex = result.index + match.length;
      }
      const stringAfterMatches = text.substring(currentIndex);
      if (stringAfterMatches) {
        doSplit(stringAfterMatches, regexIndex + 1, startIndex + currentIndex);
      }
    }
  },
};

export class FilterParser {
  private readonly keys: string[];
  constructor(keys: string[]) {
    this.keys = keys;
  }

  static cloneFilter(filter: ParsedFilter): ParsedFilter {
    return {key: filter.key, text: filter.text, regex: filter.regex, negative: filter.negative};
  }

  parse(query: string): ParsedFilter[] {
    const splitFilters = Utils.splitStringByRegexes(
        query, [Utils._keyValueFilterRegex, Utils._regexFilterRegex, Utils._textFilterRegex]);
    const parsedFilters: ParsedFilter[] = [];
    for (const {regexIndex, captureGroups} of splitFilters) {
      if (regexIndex === -1) {
        continue;
      }
      if (regexIndex === 0) {
        const startsWithMinus = captureGroups[0];
        const parsedKey = captureGroups[1];
        const parsedValue = captureGroups[2];
        if (this.keys.indexOf((parsedKey as string)) !== -1) {
          parsedFilters.push({
            key: parsedKey,
            regex: undefined,
            text: parsedValue,
            negative: Boolean(startsWithMinus),
          });
        } else {
          parsedFilters.push({
            key: undefined,
            regex: undefined,
            text: `${parsedKey}:${parsedValue}`,
            negative: Boolean(startsWithMinus),
          });
        }
      } else if (regexIndex === 1) {
        const startsWithMinus = captureGroups[0];
        const parsedRegex = captureGroups[1];
        try {
          parsedFilters.push({
            key: undefined,
            regex: new RegExp((parsedRegex as string), 'i'),
            text: undefined,
            negative: Boolean(startsWithMinus),
          });
        } catch (e) {
          parsedFilters.push({
            key: undefined,
            regex: undefined,
            text: `/${parsedRegex}/`,
            negative: Boolean(startsWithMinus),
          });
        }
      } else if (regexIndex === 2) {
        const startsWithMinus = captureGroups[0];
        const parsedText = captureGroups[1];
        parsedFilters.push({
          key: undefined,
          regex: undefined,
          text: parsedText,
          negative: Boolean(startsWithMinus),
        });
      }
    }
    return parsedFilters;
  }
}

export class BalancedJSONTokenizer {
  private readonly callback: (arg0: string) => void;
  private index: number;
  private balance: number;
  private buffer: string;
  private findMultiple: boolean;
  private closingDoubleQuoteRegex: RegExp;
  private lastBalancedIndex?: number;
  constructor(callback: (arg0: string) => void, findMultiple?: boolean) {
    this.callback = callback;
    this.index = 0;
    this.balance = 0;
    this.buffer = '';
    this.findMultiple = findMultiple || false;
    this.closingDoubleQuoteRegex = /[^\\](?:\\\\)*"/g;
  }

  write(chunk: string): boolean {
    this.buffer += chunk;
    const lastIndex = this.buffer.length;
    const buffer = this.buffer;
    let index;
    for (index = this.index; index < lastIndex; ++index) {
      const character = buffer[index];
      if (character === '"') {
        this.closingDoubleQuoteRegex.lastIndex = index;
        if (!this.closingDoubleQuoteRegex.test(buffer)) {
          break;
        }
        index = this.closingDoubleQuoteRegex.lastIndex - 1;
      } else if (character === '{') {
        ++this.balance;
      } else if (character === '}') {
        --this.balance;
        if (this.balance < 0) {
          this.reportBalanced();
          return false;
        }
        if (!this.balance) {
          this.lastBalancedIndex = index + 1;
          if (!this.findMultiple) {
            break;
          }
        }
      } else if (character === ']' && !this.balance) {
        this.reportBalanced();
        return false;
      }
    }
    this.index = index;
    this.reportBalanced();
    return true;
  }

  private reportBalanced(): void {
    if (!this.lastBalancedIndex) {
      return;
    }
    this.callback(this.buffer.slice(0, this.lastBalancedIndex));
    this.buffer = this.buffer.slice(this.lastBalancedIndex);
    this.index -= this.lastBalancedIndex;
    this.lastBalancedIndex = 0;
  }

  remainder(): string {
    return this.buffer;
  }
}

export interface TokenizerFactory {
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  createTokenizer(mimeType: string):
      (arg0: string, arg1: (arg0: string, arg1: string|null, arg2: number, arg3: number) => void) => void;
}

export function isMinified(text: string): boolean {
  const kMaxNonMinifiedLength = 500;
  let linesToCheck = 10;
  let lastPosition = 0;
  do {
    let eolIndex = text.indexOf('\n', lastPosition);
    if (eolIndex < 0) {
      eolIndex = text.length;
    }
    if (eolIndex - lastPosition > kMaxNonMinifiedLength && text.substr(lastPosition, 3) !== '//#') {
      return true;
    }
    lastPosition = eolIndex + 1;
  } while (--linesToCheck >= 0 && lastPosition < text.length);

  // Check the end of the text as well
  linesToCheck = 10;
  lastPosition = text.length;
  do {
    let eolIndex = text.lastIndexOf('\n', lastPosition);
    if (eolIndex < 0) {
      eolIndex = 0;
    }
    if (lastPosition - eolIndex > kMaxNonMinifiedLength && text.substr(lastPosition, 3) !== '//#') {
      return true;
    }
    lastPosition = eolIndex - 1;
  } while (--linesToCheck >= 0 && lastPosition > 0);
  return false;
}

export const performSearchInContent = function(
    content: string, query: string, caseSensitive: boolean, isRegex: boolean): SearchMatch[] {
  const regex = Platform.StringUtilities.createSearchRegex(query, caseSensitive, isRegex);

  const text = new Text(content);
  const result = [];
  for (let i = 0; i < text.lineCount(); ++i) {
    const lineContent = text.lineAt(i);
    regex.lastIndex = 0;
    const match = regex.exec(lineContent);
    if (match) {
      result.push(new SearchMatch(i, lineContent, match.index));
    }
  }
  return result;
};
export interface ParsedFilter {
  key?: string;
  text?: string|null;
  regex?: RegExp;
  negative: boolean;
}
