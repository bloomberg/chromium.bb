// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../../../../front_end/core/common/common.js';
import * as CodeMirror from '../../../../../../front_end/third_party/codemirror.next/codemirror.next.js';
import * as TextEditor from '../../../../../../front_end/ui/components/text_editor/text_editor.js';
import {renderElementIntoDOM} from '../../../helpers/DOMHelpers.js';
import {describeWithEnvironment} from '../../../helpers/EnvironmentHelpers.js';

const {assert} = chai;

function makeState(doc: string, extensions: CodeMirror.Extension = []) {
  return CodeMirror.EditorState.create({doc, extensions: [extensions, TextEditor.Config.baseConfiguration(doc)]});
}

describeWithEnvironment('TextEditor', () => {
  describe('component', () => {
    it('has a state property', () => {
      const editor = new TextEditor.TextEditor.TextEditor(makeState('one'));
      assert.strictEqual(editor.state.doc.toString(), 'one');
      editor.state = makeState('two');
      assert.strictEqual(editor.state.doc.toString(), 'two');
      renderElementIntoDOM(editor);
      assert.strictEqual(editor.editor.state.doc.toString(), 'two');
      editor.editor.dispatch({changes: {from: 3, insert: '!'}});
      editor.remove();
      assert.strictEqual(editor.editor.state.doc.toString(), 'two!');
    });

    it('sets an aria-label attribute', () => {
      const editor = new TextEditor.TextEditor.TextEditor(makeState(''));
      assert.strictEqual(editor.editor.contentDOM.getAttribute('aria-label'), 'Code editor');
    });

    it('can highlight whitespace', () => {
      const editor = new TextEditor.TextEditor.TextEditor(
          makeState('line1  \n  line2( )\n\tline3  ', TextEditor.Config.showWhitespace));
      renderElementIntoDOM(editor);
      assert.strictEqual(editor.editor.dom.querySelectorAll('.cm-trailingWhitespace, .cm-highlightedSpaces').length, 0);
      Common.Settings.Settings.instance().moduleSetting('showWhitespacesInEditor').set('all');
      assert.strictEqual(editor.editor.dom.querySelectorAll('.cm-highlightedSpaces').length, 4);
      assert.strictEqual(editor.editor.dom.querySelectorAll('.cm-highlightedTab').length, 1);
      Common.Settings.Settings.instance().moduleSetting('showWhitespacesInEditor').set('trailing');
      assert.strictEqual(editor.editor.dom.querySelectorAll('.cm-highlightedSpaces').length, 0);
      assert.strictEqual(editor.editor.dom.querySelectorAll('.cm-trailingWhitespace').length, 2);
      Common.Settings.Settings.instance().moduleSetting('showWhitespacesInEditor').set('none');
      assert.strictEqual(editor.editor.dom.querySelectorAll('.cm-trailingWhitespace, .cm-highlightedSpaces').length, 0);
      editor.remove();
    });
  });

  describe('configuration', () => {
    it('can guess indentation', () => {
      assert.strictEqual(
          TextEditor.Config.guessIndent(CodeMirror.Text.of(['hello():', '    world();', '    return;'])), '    ');
      assert.strictEqual(
          TextEditor.Config.guessIndent(CodeMirror.Text.of(['hello():', '\tworld();', '\treturn;'])), '\t');
    });

    it('can detect line separators', () => {
      assert.strictEqual(makeState('one\r\ntwo\r\nthree').lineBreak, '\r\n');
      assert.strictEqual(makeState('one\ntwo\nthree').lineBreak, '\n');
      assert.strictEqual(makeState('one\r\ntwo\nthree').lineBreak, '\n');
    });

    it('handles dynamic reconfiguration', () => {
      const editor = new TextEditor.TextEditor.TextEditor(makeState(''));
      renderElementIntoDOM(editor);

      assert.strictEqual(editor.state.facet(CodeMirror.indentUnit), '    ');
      Common.Settings.Settings.instance().moduleSetting('textEditorIndent').set('\t');
      assert.strictEqual(editor.state.facet(CodeMirror.indentUnit), '\t');
      Common.Settings.Settings.instance().moduleSetting('textEditorIndent').set('    ');
    });
  });
});
