// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const path = require('path');
const fs = require('fs');
const glob = require('glob');

const args = process.argv.slice(2);
const shouldCheckAllFiles = args.some(arg => arg === '--check-all-files');
const darkModeScript = path.join(__dirname, 'generate_dark_theme_sheet.js');
const errors = [];

if (shouldCheckAllFiles) {
  // Rather than glob for every *.css file and check if it has a dark mode file,
  // we glob for every *.darkmode.css file.
  const files = glob.sync('front_end/**/*.darkmode.css');
  files.forEach(file => checkCSSFileIsUpToDate(file));
} else {
  // If we don't get the --check-all-files flag, the args are a list of CSS files to chekc.
  const changedCSSFiles = args;
  changedCSSFiles.forEach(file => {
    checkCSSFileIsUpToDate(file);
  });
}

function checkCSSFileIsUpToDate(file) {
  if (file.includes(path.join('front_end', 'third_party'))) {
    return;
  }

  let sourceFile;
  let darkModeFile;
  if (file.endsWith('.darkmode.css')) {
    darkModeFile = file;
    sourceFile = path.join(path.dirname(file), path.basename(file, '.darkmode.css') + '.css');
  } else {
    sourceFile = file;
    darkModeFile = path.join(path.dirname(file), path.basename(file, '.css') + '.darkmode.css');
  }

  const sourceExists = fs.existsSync(sourceFile);
  const darkModeExists = fs.existsSync(darkModeFile);

  const sourcePathRelative = path.relative(process.cwd(), sourceFile);
  const darkModePathRelative = path.relative(process.cwd(), darkModeFile);
  if (!sourceExists && darkModeExists) {
    // If the dark file exists, but the source does not, the source must have been deleted, so the dark mode file should go.
    errors.push(`Found dark mode file ${darkModePathRelative} for deleted source file. ${
        darkModePathRelative} should be deleted.`);
  } else if (sourceExists && darkModeExists) {
    // If the source & dark mode exist, check if the source is more recent than the dark mode, and if so, we need to regen the dark mode.
    const sourceFileMTime = fs.statSync(sourceFile).mtime;
    const darkFileMTime = fs.statSync(darkModeFile).mtime;
    const generationScriptMTime = fs.statSync(darkModeScript).mtime;
    // If the source file was modified more recently than the dark file, the dark file needs to be regenerated.
    if (sourceFileMTime > darkFileMTime) {
      errors.push(`${sourcePathRelative} has been modified (${sourceFileMTime}) and ${
          darkModePathRelative} should be regenerated (${darkFileMTime}).`);
    } else if (generationScriptMTime > darkFileMTime) {
      // Disabled due to false positives. See https://crbug.com/1198054 for details.
      // TODO(jacktfranklin): fix above bug and re-enable this check.
      // The generation script itself has been changed so we must rerun.
      // errors.push(`generate_dark_theme_sheet.js has been modified and ${darkModePathRelative} should be regenerated.`);
    }
  }
}

if (errors.length) {
  console.error('Dark mode stylesheet errors:');
  console.error(errors.join('\n'));
  process.exit(1);
}
