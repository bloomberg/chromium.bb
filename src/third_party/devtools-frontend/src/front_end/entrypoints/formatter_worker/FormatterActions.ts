// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const enum FormatterActions {
  FORMAT = 'format',
  PARSE_CSS = 'parseCSS',
  HTML_OUTLINE = 'htmlOutline',
  JAVASCRIPT_OUTLINE = 'javaScriptOutline',
  JAVASCRIPT_IDENTIFIERS = 'javaScriptIdentifiers',
  JAVASCRIPT_SUBSTITUTE = 'javaScriptSubstitute',
  EVALUATE_JAVASCRIPT_SUBSTRING = 'evaluatableJavaScriptSubstring',
  ARGUMENTS_LIST = 'argumentsList',
}

export interface FormatMapping {
  original: number[];
  formatted: number[];
}

export interface FormatResult {
  content: string;
  mapping: FormatMapping;
}
