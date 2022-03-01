// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as CodeHighlighter from '../../../../../front_end/ui/components/code_highlighter/code_highlighter.js';

const {assert} = chai;

function parseTokens(code: string): [string, string][] {
  const token = /\[(\S+) ([^\]]+)\]/g, tokens: [string, string][] = [];
  for (let pos = 0;;) {
    const match = token.exec(code);
    const next = match ? match.index : code.length;
    if (next > pos) {
      tokens.push([code.slice(pos, next), '']);
    }
    if (!match) {
      break;
    }
    tokens.push([match[2], match[1]]);
    pos = match.index + match[0].length;
  }
  return tokens;
}

function testHighlight(code: string, mimeType: string) {
  return async () => {
    const tokens = parseTokens(code), rawCode = tokens.map(t => t[0]).join('');
    const highlighter = await CodeHighlighter.CodeHighlighter.create(rawCode, mimeType);
    let i = 0;
    highlighter.highlight((text, style): void => {
      assert.strictEqual(
          JSON.stringify([text, style.replace(/\btoken-/g, '').split(' ').sort().join('&')]),
          JSON.stringify(tokens[i++] || ['', '']));
    });
  };
}

describe('CodeHighlighter', () => {
  // clang-format off
  it('can highlight JavaScript', testHighlight(`
[keyword function] [variable foo]([variable bar]) {
  [keyword return] [number 22];
}`, 'text/javascript'));

  it('can highlight TypeScript', testHighlight(`
[keyword type] [type X] = {
  [property x]: [type boolean]
}`, 'text/typescript'));

  it('can highlight JSX', testHighlight(`
[keyword const] [variable t] = <[tag div] [attribute disabled]>hello</[tag div]>
`, 'text/jsx'));

  it('can highlight HTML', testHighlight(`
[meta <!doctype html>]
<[tag html] [attribute lang]=[attribute-value ar]>
  ...
</[tag html]>`, 'text/html'));

  it('can highlight CSS', testHighlight(`
[tag span].[type cls]#[atom id] {
  [property font-weight]: [atom bold];
  [property color]: [number #ff2];
  [property width]: [number 4px];
}`, 'text/css'));

  it('can highlight WAST', testHighlight(`
([keyword module]
 ([keyword type] [variable $t] ([keyword func] ([keyword param] [type i32])))
 ([keyword func] [variable $max] [comment (; 1 ;)] ([keyword param] [variable $0] [type i32]) ([keyword result] [type i32])
   ([keyword get_local] [variable $0])))
`, 'text/webassembly'));

  it('can highlight JSON', testHighlight(`
{
  [property "one"]: [number 2],
  [property "two"]: [atom true]
}`, 'application/json'));

  it('can highlight Markdown', testHighlight(`
[heading&meta #][heading  Head]

Paragraph with [emphasis&meta *][emphasis emphasized][emphasis&meta *] text.
`, 'text/markdown'));

  it('can highlight Python', testHighlight(`
[keyword def] [variable f]([variable x] = [atom True]):
  [keyword return] [variable x] * [number 10];
`, 'text/x-python'));

  it('can highlight Shell code', testHighlight(`
[builtin cat] [string "a"]
`, 'text/x-sh'));
  // clang_format on
});
