// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const enum FormatterActions {
  FORMAT = 'format',
  PARSE_CSS = 'parseCSS',
  HTML_OUTLINE = 'htmlOutline',
  JAVASCRIPT_OUTLINE = 'javaScriptOutline',
  JAVASCRIPT_IDENTIFIERS = 'javaScriptIdentifiers',
  EVALUATE_JAVASCRIPT_SUBSTRING = 'evaluatableJavaScriptSubstring',
  FIND_LAST_EXPRESSION = 'findLastExpression',
  FIND_LAST_FUNCTION_CALL = 'findLastFunctionCall',
  ARGUMENTS_LIST = 'argumentsList',
}
