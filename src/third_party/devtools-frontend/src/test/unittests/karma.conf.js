// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-nocheck File doesn't need to be checked by TS.

const path = require('path');
const glob = require('glob');
const fs = require('fs');
const rimraf = require('rimraf');

// false by default
const DEBUG_ENABLED = !!process.env['DEBUG'];
const COVERAGE_ENABLED = !!process.env['COVERAGE'];

// true by default
const TEXT_COVERAGE_ENABLED = COVERAGE_ENABLED && !process.env['NO_TEXT_COVERAGE'];
const COVERAGE_OUTPUT_DIRECTORY = 'karma-coverage';

if (COVERAGE_ENABLED) {
  /* Clear out the old coverage directory so you can't accidentally open old,
   * out of date coverage output.
   */
  const fullPathToDirectory = path.resolve(process.cwd(), COVERAGE_OUTPUT_DIRECTORY);
  if (fs.existsSync(fullPathToDirectory)) {
    rimraf.sync(fullPathToDirectory);
  }
}

const GEN_DIRECTORY = path.join(__dirname, '..', '..');
const ROOT_DIRECTORY = path.join(GEN_DIRECTORY, '..', '..', '..');
const browsers = DEBUG_ENABLED ? ['Chrome'] : ['ChromeHeadless'];

const coverageReporters = COVERAGE_ENABLED ? ['coverage'] : [];
const coveragePreprocessors = COVERAGE_ENABLED ? ['coverage'] : [];
const commonIstanbulReporters = [{type: 'html'}, {type: 'json-summary'}];
const istanbulReportOutputs =
    TEXT_COVERAGE_ENABLED ? [{type: 'text'}, ...commonIstanbulReporters] : commonIstanbulReporters;

const UNIT_TESTS_ROOT_FOLDER = path.join(ROOT_DIRECTORY, 'test', 'unittests');
const UNIT_TESTS_FOLDERS = [
  path.join(UNIT_TESTS_ROOT_FOLDER, 'front_end'),
  path.join(UNIT_TESTS_ROOT_FOLDER, 'inspector_overlay'),
];
const TEST_SOURCES = UNIT_TESTS_FOLDERS.map(folder => path.join(folder, '**/*.ts'));

// To make sure that any leftover JavaScript files (e.g. that were outputs from now-removed tests)
// aren't incorrectly included, we glob for the TypeScript files instead and use that
// to instruct Mocha to run the output JavaScript file.
const TEST_FILES =
    TEST_SOURCES
        .map(source => {
          return glob.sync(source).map(fileName => {
            const jsFile = fileName.replace(/\.ts$/, '.js');
            const generatedJsFile = path.join(__dirname, path.relative(UNIT_TESTS_ROOT_FOLDER, jsFile));
            if (!fs.existsSync(generatedJsFile)) {
              throw new Error(`Test file ${fileName} is not included in a BUILD.gn and therefore will not be run.`);
            }

            return generatedJsFile;
          });
        })
        .flat();


const TEST_FILES_SOURCE_MAPS = TEST_FILES.map(fileName => `${fileName}.map`);

// Locate the test setup file in all the gathered files. This is so we can
// ensure that it goes first and registers its global hooks before anything else.
const testSetupFilePattern = {
  pattern: null,
  type: 'module'
};
const testFiles = [];
for (const pattern of TEST_FILES) {
  if (pattern.endsWith('test_setup.js')) {
    testSetupFilePattern.pattern = pattern;
  } else {
    testFiles.push({pattern, type: 'module'});
  }
}

module.exports = function(config) {
  const options = {
    basePath: ROOT_DIRECTORY,

    files: [
      // Ensure the test setup goes first because Karma registers with Mocha in file order, and the hooks in the test_setup
      // must be set before any other hooks in order to ensure all tests get the same environment.
      testSetupFilePattern,
      ...testFiles,
      ...TEST_FILES_SOURCE_MAPS.map(pattern => ({pattern, served: true, included: false})),
      ...TEST_SOURCES.map(source => ({pattern: source, served: true, included: false})),
      {pattern: path.join(GEN_DIRECTORY, 'front_end/Images/*.{svg,png}'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'front_end/**/*.css'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'front_end/**/*.js'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'front_end/**/*.js.map'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'front_end/**/*.mjs'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'front_end/**/*.mjs.map'), served: true, included: false},
      {pattern: path.join(ROOT_DIRECTORY, 'front_end/**/*.ts'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'inspector_overlay/**/*.js'), served: true, included: false},
      {pattern: path.join(GEN_DIRECTORY, 'inspector_overlay/**/*.js.map'), served: true, included: false},
    ],

    reporters: [
      'dots',
      ...coverageReporters,
    ],

    browsers,

    frameworks: ['mocha', 'chai'],

    client: {
      /*
       * Passed through to the client via __karma__ because test_setup.ts
       * preloads some CSS files and it needs to know the target directory to do
       * so.
       */
      targetDir: path.relative(process.cwd(), GEN_DIRECTORY),
    },

    plugins: [
      require('karma-chrome-launcher'),
      require('karma-mocha'),
      require('karma-chai'),
      require('karma-sourcemap-loader'),
      require('karma-coverage'),
    ],

    preprocessors: {
      '**/*.{js,mjs}': ['sourcemap'],
      [path.join(GEN_DIRECTORY, 'front_end/!(third_party)/**/!(wasm_source_map|*_bridge).{js,mjs}')]:
          [...coveragePreprocessors],
    },

    proxies: {'/Images': 'front_end/Images'},

    coverageReporter: {dir: COVERAGE_OUTPUT_DIRECTORY, reporters: istanbulReportOutputs},

    singleRun: !DEBUG_ENABLED,
  };

  config.set(options);
};
