// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../third_party/codemirror/package/addon/runmode/runmode-standalone.js';
import '../../third_party/codemirror/package/mode/css/css.js';
import '../../third_party/codemirror/package/mode/xml/xml.js';
import '../../third_party/codemirror/package/mode/javascript/javascript.js';

import * as Platform from '../../core/platform/platform.js';
import * as FormatterWorker from './formatter_worker.js';  // eslint-disable-line rulesdir/es_modules_import

import {FormatterActions} from './FormatterActions.js';

self.onmessage = function(event: MessageEvent): void {
  const method: FormatterActions = event.data.method;
  const params: {indentString: string, content: string, mimeType: string} = event.data.params;
  if (!method) {
    return;
  }

  switch (method) {
    case FormatterActions.FORMAT:
      self.postMessage(FormatterWorker.FormatterWorker.format(params.mimeType, params.content, params.indentString));
      break;
    case FormatterActions.PARSE_CSS:
      FormatterWorker.CSSRuleParser.parseCSS(params.content, self.postMessage);
      break;
    case FormatterActions.HTML_OUTLINE:
      FormatterWorker.HTMLOutline.htmlOutline(params.content, self.postMessage);
      break;
    case FormatterActions.JAVASCRIPT_OUTLINE:
      FormatterWorker.JavaScriptOutline.javaScriptOutline(params.content, self.postMessage);
      break;
    case FormatterActions.JAVASCRIPT_IDENTIFIERS:
      self.postMessage(FormatterWorker.FormatterWorker.javaScriptIdentifiers(params.content));
      break;
    case FormatterActions.EVALUATE_JAVASCRIPT_SUBSTRING:
      self.postMessage(FormatterWorker.FormatterWorker.evaluatableJavaScriptSubstring(params.content));
      break;
    case FormatterActions.FIND_LAST_EXPRESSION:
      self.postMessage(FormatterWorker.FormatterWorker.findLastExpression(params.content));
      break;
    case FormatterActions.FIND_LAST_FUNCTION_CALL:
      self.postMessage(FormatterWorker.FormatterWorker.findLastFunctionCall(params.content));
      break;
    case FormatterActions.ARGUMENTS_LIST:
      self.postMessage(FormatterWorker.FormatterWorker.argumentsList(params.content));
      break;
    default:
      Platform.assertNever(method, `Unsupport method name: ${method}`);
  }
};

self.postMessage('workerReady');
