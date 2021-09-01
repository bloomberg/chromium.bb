// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const path = require('path');

const FRONT_END_DIRECTORY = path.join(__dirname, '..', '..', '..', 'front_end');

function isModuleScope(context) {
  return context.getScope().type === 'module';
}

// True iff the callExpression is `i18n.i18n.registerUIStrings`.
function isRegisterUIStringsCall(callExpression) {
  if (callExpression.callee?.property?.name !== 'registerUIStrings') {
    return false;
  }

  if (callExpression.callee?.object?.property?.name !== 'i18n') {
    return false;
  }

  if (callExpression.callee?.object?.object?.name !== 'i18n') {
    return false;
  }
  return true;
}

module.exports = {
  meta: {
    type: 'problem',
    docs: {
      description: 'i18n.i18n.registerUIStrings must receive the current file\'s path as the first argument',
      category: 'Possible Errors',
    },
    fixable: 'code',
    schema: []  // no options
  },
  create: function(context) {
    return {
      CallExpression(callExpression) {
        if (!isModuleScope(context) || !isRegisterUIStringsCall(callExpression)) {
          return;
        }

        // Do nothing if there are no arguments or the first argument is not a string literal we
        // can check.
        if (callExpression.arguments.length === 0 || callExpression.arguments[0].type !== 'Literal') {
          return;
        }

        const currentSourceFile = path.resolve(context.getFilename());
        const currentFileRelativeToFrontEnd = path.relative(FRONT_END_DIRECTORY, currentSourceFile);

        const currentModuleDirectory = path.dirname(currentSourceFile);
        const allowedPathArguments = [
          currentSourceFile,
          path.join(currentModuleDirectory, 'ModuleUIStrings.js'),
          path.join(currentModuleDirectory, 'ModuleUIStrings.ts'),
        ];

        const previousFileLocationArgument = callExpression.arguments[0];
        const actualPath = path.join(FRONT_END_DIRECTORY, previousFileLocationArgument.value);
        if (!allowedPathArguments.includes(actualPath)) {
          context.report({
            node: callExpression,
            message: `First argument to 'registerUIStrings' call must be '${
                currentFileRelativeToFrontEnd}' or the ModuleUIStrings.(js|ts)`,
            fix(fixer) {
              return fixer.replaceText(previousFileLocationArgument, `'${currentFileRelativeToFrontEnd}'`);
            }
          });
        }
      }
    };
  }
};
