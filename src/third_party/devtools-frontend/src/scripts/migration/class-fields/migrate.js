// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Project, ts, SyntaxKind, StructureKind} = require('ts-morph');
const path = require('path');
  const project = new Project({
  tsConfigFilePath: '../devtools-frontend/tsconfig.json',
});

function isWithinContructor(classNode, reference) {
  const constructors = classNode.getConstructors();
  if (!constructors.length) {
    return false;
  }

  if (constructors.length !== 1) {
    throw new Error('More than one constructor');
  }
  return reference.getAncestors().includes(constructors[0]);
}

const files = project.getSourceFiles();
for (const file of files) {
  const filePath = file.getFilePath();
  if (!filePath.startsWith(
          path.normalize(path.join(process.cwd(), '..', 'devtools-frontend', 'front_end', process.argv[2])))) {
    continue;
  }
  console.log(filePath);
  if (filePath.includes('third_party') || filePath.includes('generated') || filePath.endsWith('.d.ts')) {
    continue;
  }
  const classNodes = file.getClasses();
  for (const classNode of classNodes) {
    for (const property of classNode.getInstanceProperties()) {
      const name = property.getStructure().name;
      console.log(name);
      if (name.startsWith('#') || !property.hasModifier(SyntaxKind.PrivateKeyword)) {
        continue;
      }

      if (isWithinContructor(classNode, property)) {
        continue;
      }

      property.toggleModifier('private', false);
      // https://github.com/dsherret/ts-morph/issues/1198
      property.rename(`SOME_STUPID_PREFIX_${name}`, {
        renameInComments: true,
      });
    }
  }
  file.saveSync();
}
// project.save();
