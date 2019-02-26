// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

'use strict';

// https://wicg.github.io/animation-worklet/

idl_test(
  ['animation-worklet'],
  ['worklets', 'web-animations', 'html', 'cssom', 'dom'],
  idl_array => {
    idl_array.add_objects({
      WorkletAnimation: ['new WorkletAnimation("name")'],
      // TODO: WorkletGroupEffect
    });
  }
);
