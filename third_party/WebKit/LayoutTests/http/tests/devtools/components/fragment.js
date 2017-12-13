// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  function check(f) {
    TestRunner.addResult(f.toString() + '\n  ' + f());
  }

  TestRunner.addResult(`Tests how fragment works.\n`);

  var inner = document.createElement('div');
  var f1 = UI.Fragment.build`
    <div-a $=name-a attr=val s-state1-attr=val-state1 s-state2-attr=val-state2>
      <x-shadow $=name-shadow>
        <div-b $=name-b foo1=bar1 foo${'2'}=${'b'}ar${'2'} ${''} ${element => element.divb = true} s-state1-attr=val-state1>
        </div-b>
      </x-shadow>
      <div-c $=name-c class='${'my-class-1'} my-class-2' ${'foo'}=bar>${'Some text here'} ${'And more text'}</div-c>
      ${inner}
    </div-a>
  `;

  TestRunner.addResult('f1.outerHTML:');
  TestRunner.addResult(f1.element().outerHTML);

  var diva = f1.$('name-a');
  var divb = f1.$('name-b');
  var divc = f1.$('name-c');
  var shadow = f1.$('name-shadow');

  check(() => diva === f1.element());
  check(() => diva.tagName === 'DIV-A');
  check(() => divb.tagName === 'DIV-B');
  check(() => divc.tagName === 'DIV-C');
  check(() => shadow.nodeType === Node.DOCUMENT_FRAGMENT_NODE);
  check(() => shadow.parentElementOrShadowHost() === diva);
  check(() => divc.parentNode === diva);
  check(() => divb.parentElementOrShadowHost() === diva);
  check(() => diva.lastChild === inner);

  check(() => divb.getAttribute('foo1') === 'bar1');
  check(() => divb.getAttribute('foo2') === 'bar2');
  check(() => divb.divb === true);

  check(() => divc.textContent === 'Some text here And more text');
  check(() => divc.classList.contains('my-class-1'));
  check(() => divc.classList.contains('my-class-2'));
  check(() => divc.getAttribute('foo') === 'bar');

  check(() => diva.getAttribute('attr') === 'val');
  check(() => divb.getAttribute('attr') === null);
  f1.setState('state1', true);
  check(() => diva.getAttribute('attr') === 'val-state1');
  check(() => divb.getAttribute('attr') === 'val-state1');
  f1.setState('state1', true);
  check(() => diva.getAttribute('attr') === 'val-state1');
  check(() => divb.getAttribute('attr') === 'val-state1');
  f1.setState('state1', false);
  check(() => diva.getAttribute('attr') === 'val');
  check(() => divb.getAttribute('attr') === null);
  f1.setState('state2', true);
  check(() => diva.getAttribute('attr') === 'val-state2');
  check(() => divb.getAttribute('attr') === null);

  TestRunner.addResult('');

  function cached(child) {
    return UI.Fragment.cached`
      <div>${child}</div>
    `;
  }

  var f2 = cached(f1);
  TestRunner.addResult('f2.outerHTML:');
  TestRunner.addResult(f2.element().outerHTML);
  check(() => f2.element().firstChild === f1.element());
  TestRunner.addResult('');

  var f3 = cached(f2);
  TestRunner.addResult('f3.outerHTML:');
  TestRunner.addResult(f3.element().outerHTML);
  check(() => f3.element().firstChild === f2.element());
  TestRunner.addResult('');

  TestRunner.completeTest();
})();
