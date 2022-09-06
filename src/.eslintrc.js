// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'root': true,
  'env': {
    'browser': true,
    'es2020': true,
  },
  'parserOptions': {
    'ecmaVersion': 2020,
    'sourceType': 'module',
  },
  'rules': {
    // Enabled checks.
    'brace-style': ['error', '1tbs'],
    'curly': ['error', 'multi-line', 'consistent'],
    'new-parens': 'error',
    'no-console': ['error', {allow: ['info', 'warn', 'error', 'assert']}],
    'no-extra-boolean-cast': 'error',
    'no-extra-semi': 'error',
    'no-new-wrappers': 'error',
    'no-restricted-properties': [
      'error',
      {
        'property': '__lookupGetter__',
        'message': 'Use Object.getOwnPropertyDescriptor',
      },
      {
        'property': '__lookupSetter__',
        'message': 'Use Object.getOwnPropertyDescriptor',
      },
      {
        'property': '__defineGetter__',
        'message': 'Use Object.defineProperty',
      },
      {
        'property': '__defineSetter__',
        'message': 'Use Object.defineProperty',
      },
      {
        'object': 'cr',
        'property': 'exportPath',
        'message': 'Use ES modules or cr.define() instead',
      },
    ],
    'no-throw-literal': 'error',
    'no-trailing-spaces': 'error',
    'no-var': 'error',
    'prefer-const': 'error',
    'quotes': ['error', 'single', {allowTemplateLiterals: true}],
    'semi': ['error', 'always'],

    // TODO(dpapad): Add more checks according to our styleguide.
  },

  'overrides': [{
    'files': ['**/*.ts'],
    'parser': './third_party/node/node_modules/@typescript-eslint/parser',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      'no-unused-vars': 'off',
      '@typescript-eslint/no-unused-vars': [
        'error', {
          argsIgnorePattern: '^_',
          varsIgnorePattern: '^_',
        }
      ],

      'semi': 'off',
      '@typescript-eslint/semi': ['error'],

      // https://google.github.io/styleguide/jsguide.html#naming
      '@typescript-eslint/naming-convention': [
        'error',
        {
          selector: ['class', 'interface', 'typeAlias', 'enum', 'typeParameter'],
          format: ['PascalCase'],
        },
        {
          selector: 'enumMember',
          format: ['UPPER_CASE'],
        },
        {
          selector: 'classMethod',
          format: ['camelCase'],
          leadingUnderscore: 'allow',
          trailingUnderscore: 'allow',
        },
        {
          selector: 'classProperty',
          format: ['UPPER_CASE'],
          modifiers: ['static', 'readonly'],
        },
        {
          selector: 'classProperty',
          format: ['camelCase'],
          trailingUnderscore: 'allow',
        },
        {
          selector: 'parameter',
          format: ['camelCase'],
          leadingUnderscore: 'allow',
        },
        {
          selector: 'function',
          format: ['camelCase'],
        },
      ],

      '@typescript-eslint/member-delimiter-style': ['error', {
        multiline: {
          delimiter: 'comma',
          requireLast: true,
        },
        singleline: {
          delimiter: 'comma',
          requireLast: false,
        },
        overrides: {
          interface: {
            multiline: {
              delimiter: 'semi',
              requireLast: true,
            },
            singleline: {
              delimiter: 'semi',
              requireLast: false,
            },
          },
        },
      }]
    }
  }]
};
