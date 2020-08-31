// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for paragraph_utils.js.
 */
SelectToSpeakParagraphUnitTest = class extends testing.Test {};

/** @override */
SelectToSpeakParagraphUnitTest.prototype.extraLibraries =
    ['test_support.js', 'paragraph_utils.js'];


TEST_F('SelectToSpeakParagraphUnitTest', 'GetFirstBlockAncestor', function() {
  const root = {role: 'rootWebArea'};
  const paragraph = {role: 'paragraph', parent: root, root};
  const text1 = {role: 'staticText', parent: paragraph, display: 'block', root};
  const text2 = {role: 'staticText', parent: root, root};
  const text3 = {role: 'inlineTextBox', parent: text1, root};
  const div =
      {role: 'genericContainer', parent: paragraph, display: 'block', root};
  const text4 = {role: 'staticText', parent: div, root};
  assertEquals(paragraph, ParagraphUtils.getFirstBlockAncestor(text1));
  assertEquals(root, ParagraphUtils.getFirstBlockAncestor(text2));
  assertEquals(paragraph, ParagraphUtils.getFirstBlockAncestor(text3));
  assertEquals(div, ParagraphUtils.getFirstBlockAncestor(text4));
});

TEST_F('SelectToSpeakParagraphUnitTest', 'SVGRootIsBlockAncestor', function() {
  const root = {role: 'rootWebArea'};
  const svgRoot = {role: 'svgRoot', parent: root, root};
  const text1 = {role: 'staticText', parent: svgRoot, root};
  const inline1 = {role: 'inlineTextBox', parent: text1, root};
  const text2 = {role: 'staticText', parent: svgRoot, root};
  const inline2 = {role: 'inlineTextBox', parent: text2, root};
  assertEquals(svgRoot, ParagraphUtils.getFirstBlockAncestor(text1));
  assertEquals(svgRoot, ParagraphUtils.getFirstBlockAncestor(inline1));
  assertEquals(svgRoot, ParagraphUtils.getFirstBlockAncestor(inline2));
  assertTrue(ParagraphUtils.inSameParagraph(inline1, inline2));
});


TEST_F('SelectToSpeakParagraphUnitTest', 'InSameParagraph', function() {
  const root = {role: 'rootWebArea'};
  const paragraph1 =
      {role: 'paragraph', display: 'block', parent: 'rootWebArea', root};
  const text1 = {role: 'staticText', parent: paragraph1, root};
  const text2 = {role: 'staticText', parent: paragraph1, root};
  const paragraph2 =
      {role: 'paragraph', display: 'block', parent: 'rootWebArea', root};
  const text3 = {role: 'staticText', parent: paragraph2, root};
  assertTrue(ParagraphUtils.inSameParagraph(text1, text2));
  assertFalse(ParagraphUtils.inSameParagraph(text1, text3));
});

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'BlockDivBreaksSameParagraph',
    function() {
      const root = {role: 'rootWebArea'};
      const paragraph1 =
          {role: 'paragraph', display: 'block', parent: 'rootWebArea', root};
      const text1 = {role: 'staticText', parent: paragraph1, root};
      const text2 = {role: 'image', parent: paragraph1, display: 'block', root};
      const text3 =
          {role: 'image', parent: paragraph1, display: 'inline', root};
      const text4 = {role: 'staticText', parent: paragraph1, root};
      assertFalse(ParagraphUtils.inSameParagraph(text1, text2));
      assertFalse(ParagraphUtils.inSameParagraph(text2, text3));
      assertTrue(ParagraphUtils.inSameParagraph(text3, text4));
    });

TEST_F('SelectToSpeakParagraphUnitTest', 'IsWhitespace', function() {
  assertTrue(ParagraphUtils.isWhitespace(''));
  assertTrue(ParagraphUtils.isWhitespace(' '));
  assertTrue(ParagraphUtils.isWhitespace(' \n \t '));
  assertTrue(ParagraphUtils.isWhitespace());
  assertFalse(ParagraphUtils.isWhitespace('cats'));
  assertFalse(ParagraphUtils.isWhitespace(' cats '));
});

TEST_F('SelectToSpeakParagraphUnitTest', 'GetNodeName', function() {
  assertEquals(
      ParagraphUtils.getNodeName({role: 'staticText', name: 'cat'}), 'cat');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'inlineTextBox', name: 'cat'}), 'cat');
  assertEquals(ParagraphUtils.getNodeName({name: 'cat'}), 'cat');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'radioButton', name: 'cat'}),
      'cat unselected');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'checkBox', name: 'cat'}),
      'cat unchecked');
  assertEquals(
      ParagraphUtils.getNodeName(
          {role: 'checkBox', checked: 'true', name: 'cat'}),
      'cat checked');
  assertEquals(ParagraphUtils.getNodeName({role: 'radioButton'}), 'unselected');
  assertEquals(ParagraphUtils.getNodeName({role: 'checkBox'}), 'unchecked');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'radioButton', checked: 'true'}),
      'selected');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'checkBox', checked: 'true'}),
      'checked');
  assertEquals(
      ParagraphUtils.getNodeName(
          {role: 'radioButton', checked: 'true', name: 'cat'}),
      'cat selected');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'checkBox', checked: 'mixed'}),
      'partially checked');
  assertEquals(
      ParagraphUtils.getNodeName({role: 'radioButton', checked: 'mixed'}),
      'partially selected');
});

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'GetStartCharIndexInParent', function() {
      const staticText = {
        role: 'staticText',
        name: 'My name is Bond, James Bond'
      };
      const inline1 = {
        role: 'inlineTextBox',
        name: 'My name is ',
        indexInParent: 0,
        parent: staticText
      };
      const inline2 = {
        role: 'inlineTextBox',
        name: 'Bond, ',
        indexInParent: 1,
        parent: staticText
      };
      const inline3 = {
        role: 'inlineTextBox',
        name: 'James Bond',
        indexInParent: 2,
        parent: staticText
      };
      staticText.children = [inline1, inline2, inline3];
      assertEquals(ParagraphUtils.getStartCharIndexInParent(inline1), 0);
      assertEquals(ParagraphUtils.getStartCharIndexInParent(inline2), 11);
      assertEquals(ParagraphUtils.getStartCharIndexInParent(inline3), 17);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'FindInlineTextNodeByCharIndex',
    function() {
      const staticText = {
        role: 'staticText',
        name: 'My name is Bond, James Bond'
      };
      const inline1 = {role: 'inlineTextBox', name: 'My name is '};
      const inline2 = {role: 'inlineTextBox', name: 'Bond, '};
      const inline3 = {role: 'inlineTextBox', name: 'James Bond'};
      staticText.children = [inline1, inline2, inline3];
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 0),
          inline1);
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 10),
          inline1);
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 11),
          inline2);
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 16),
          inline2);
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 17),
          inline3);
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 50),
          inline3);
      staticText.children = [];
      assertEquals(
          ParagraphUtils.findInlineTextNodeByCharacterIndex(staticText, 10),
          null);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'BuildNodeGroupStopsAtNewParagraph',
    function() {
      const root = {role: 'rootWebArea'};
      const paragraph1 =
          {role: 'paragraph', display: 'block', parent: root, root};
      const text1 =
          {role: 'staticText', parent: paragraph1, name: 'text1', root};
      const text2 =
          {role: 'staticText', parent: paragraph1, name: 'text2', root};
      const paragraph2 =
          {role: 'paragraph', display: 'block', parent: root, root};
      const text3 =
          {role: 'staticText', parent: paragraph2, name: 'text3', root};
      const result = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3], 0, false /* do not split on language */);
      assertEquals('text1 text2 ', result.text);
      assertEquals(1, result.endIndex);
      assertEquals(2, result.nodes.length);
      assertEquals(0, result.nodes[0].startChar);
      assertEquals(text1, result.nodes[0].node);
      assertEquals(6, result.nodes[1].startChar);
      assertEquals(text2, result.nodes[1].node);
      assertEquals(paragraph1, result.blockParent);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'BuildNodeGroupStopsAtLanguageBoundary',
    function() {
      const splitOnLanguage = true;

      // When the detectedLanguage changes from en-US to fr-FR we expect to
      // break the NodeGroup.
      const root = {role: 'rootWebArea'};
      const text1 = {
        role: 'staticText',
        parent: root,
        name: 'text1',
        root,
        detectedLanguage: 'en-US'
      };
      const text2 = {
        role: 'staticText',
        parent: root,
        name: 'text2',
        root,
        detectedLanguage: 'en-US'
      };
      const text3 = {
        role: 'staticText',
        parent: root,
        name: 'text3',
        root,
        detectedLanguage: 'fr-FR'
      };

      const result1 = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3], 0, splitOnLanguage);
      assertEquals('text1 text2 ', result1.text);
      assertEquals(1, result1.endIndex);
      assertEquals(2, result1.nodes.length);
      assertEquals(0, result1.nodes[0].startChar);
      assertEquals(text1, result1.nodes[0].node);
      assertEquals(6, result1.nodes[1].startChar);
      assertEquals(text2, result1.nodes[1].node);
      assertEquals('en-US', result1.detectedLanguage);

      const result2 = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3], 2, splitOnLanguage);
      assertEquals('text3 ', result2.text);
      assertEquals(2, result2.endIndex);
      assertEquals(1, result2.nodes.length);
      assertEquals(0, result2.nodes[0].startChar);
      assertEquals(text3, result2.nodes[0].node);
      assertEquals('fr-FR', result2.detectedLanguage);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest',
    'BuildNodeGroupStopsAtLanguageBoundaryAllUndefined', function() {
      const splitOnLanguage = true;

      // If no detectedLanguage is defined then we should not split at all....
      const root = {role: 'rootWebArea'};
      const text1 = {role: 'staticText', parent: root, name: 'text1', root};
      const text2 = {role: 'staticText', parent: root, name: 'text2', root};
      const text3 = {role: 'staticText', parent: root, name: 'text3', root};
      const result = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3], 0, splitOnLanguage);
      assertEquals('text1 text2 text3 ', result.text);
      assertEquals(2, result.endIndex);
      assertEquals(3, result.nodes.length);
      assertEquals(0, result.nodes[0].startChar);
      assertEquals(text1, result.nodes[0].node);
      assertEquals(6, result.nodes[1].startChar);
      assertEquals(text2, result.nodes[1].node);
      assertEquals(12, result.nodes[2].startChar);
      assertEquals(text3, result.nodes[2].node);
      assertEquals(undefined, result.detectedLanguage);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest',
    'BuildNodeGroupStopsAtLanguageBoundaryLastNode', function() {
      const splitOnLanguage = true;

      // our NodeGroup should get the first defined detectedLanguage
      const root = {role: 'rootWebArea'};
      const text1 = {role: 'staticText', parent: root, name: 'text1', root};
      const text2 = {role: 'staticText', parent: root, name: 'text2', root};
      const text3 = {
        role: 'staticText',
        parent: root,
        name: 'text3',
        root,
        detectedLanguage: 'fr-FR'
      };
      const result = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3], 0, splitOnLanguage);
      assertEquals('text1 text2 text3 ', result.text);
      assertEquals(2, result.endIndex);
      assertEquals(3, result.nodes.length);
      assertEquals(0, result.nodes[0].startChar);
      assertEquals(text1, result.nodes[0].node);
      assertEquals(6, result.nodes[1].startChar);
      assertEquals(text2, result.nodes[1].node);
      assertEquals(12, result.nodes[2].startChar);
      assertEquals(text3, result.nodes[2].node);
      assertEquals('fr-FR', result.detectedLanguage);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'BuildNodeGroupSplitOnLanguageDisabled',
    function() {
      // Test behaviour with splitOnLanguage disabled. This is to show that we
      // haven't introduced an obvious regression.
      const splitOnLanguage = false;

      const root = {role: 'rootWebArea'};
      const text1 = {role: 'staticText', parent: root, name: 'text1', root};
      const text2 = {
        role: 'staticText',
        parent: root,
        name: 'text2',
        root,
        detectedLanguage: 'en-US'
      };
      const text3 = {role: 'staticText', parent: root, name: 'text3', root};
      const text4 = {
        role: 'staticText',
        parent: root,
        name: 'text4',
        root,
        detectedLanguage: 'fr-FR'
      };
      const result = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3, text4], 0, splitOnLanguage);
      assertEquals('text1 text2 text3 text4 ', result.text);
      assertEquals(3, result.endIndex);
      assertEquals(4, result.nodes.length);
      assertEquals(text1, result.nodes[0].node);
      assertEquals(text4, result.nodes[3].node);
      assertEquals(undefined, result.detectedLanguage);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest',
    'BuildNodeGroupStopsAtLanguageBoundarySomeUndefined', function() {
      const splitOnLanguage = true;

      // We never want to break up a NodeGroup based on an undefined
      // detectedLanguage, instead we allow an undefined detectedLanguage to
      // match any other language. The language for the NodeGroup will be
      // determined by the first defined detectedLanguage.
      const root = {role: 'rootWebArea'};
      const text1 = {role: 'staticText', parent: root, name: 'text1', root};
      const text2 = {
        role: 'staticText',
        parent: root,
        name: 'text2',
        root,
        detectedLanguage: 'en-US'
      };
      const text3 = {role: 'staticText', parent: root, name: 'text3', root};
      const text4 = {
        role: 'staticText',
        parent: root,
        name: 'text4',
        root,
        detectedLanguage: 'fr-FR'
      };
      const result = ParagraphUtils.buildNodeGroup(
          [text1, text2, text3, text4], 0, splitOnLanguage);
      assertEquals('text1 text2 text3 ', result.text);
      assertEquals(2, result.endIndex);
      assertEquals(3, result.nodes.length);
      assertEquals(0, result.nodes[0].startChar);
      assertEquals(text1, result.nodes[0].node);
      assertEquals(6, result.nodes[1].startChar);
      assertEquals(text2, result.nodes[1].node);
      assertEquals(12, result.nodes[2].startChar);
      assertEquals(text3, result.nodes[2].node);
      assertEquals('en-US', result.detectedLanguage);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'BuildNodeGroupIncludesLinks',
    function() {
      const root = {role: 'rootWebArea'};
      const paragraph1 =
          {role: 'paragraph', display: 'block', parent: root, root};
      const text1 =
          {role: 'staticText', parent: paragraph1, name: 'text1', root};
      // Whitespace-only nodes should be ignored.
      const text2 = {role: 'staticText', parent: paragraph1, name: '\n', root};
      const link = {role: 'link', parent: paragraph1, root};
      const linkText =
          {role: 'staticText', parent: link, name: 'linkText', root};
      const result = ParagraphUtils.buildNodeGroup(
          [text1, text2, linkText], 0, false /* do not split on language */);
      assertEquals('text1 linkText ', result.text);
      assertEquals(2, result.endIndex);
      assertEquals(2, result.nodes.length);
      assertEquals(0, result.nodes[0].startChar);
      assertEquals(text1, result.nodes[0].node);
      assertEquals(6, result.nodes[1].startChar);
      assertEquals(linkText, result.nodes[1].node);
      assertEquals(paragraph1, result.blockParent);
    });

TEST_F(
    'SelectToSpeakParagraphUnitTest', 'BuildNodeGroupNativeTextBox',
    function() {
      const root = {role: 'desktop'};
      const parent = {role: 'pane', parent: root, root};
      const searchBar = {
        role: 'textField',
        name: 'Address and search bar',
        value: 'http://www.google.com',
        children: []
      };
      let result = ParagraphUtils.buildNodeGroup([searchBar], 0);
      assertEquals('http://www.google.com ', result.text);

      // If there is no value, it should use the name.
      searchBar.value = '';
      result = ParagraphUtils.buildNodeGroup(
          [searchBar], 0, false /* do not split on language */);
      assertEquals('Address and search bar ', result.text);
    });

TEST_F('SelectToSpeakParagraphUnitTest', 'BuildNodeGroupWithSvg', function() {
  const root = {role: 'rootWebArea'};
  const svgRoot = {role: 'svgRoot', parent: root, root};
  const text1 = {role: 'staticText', parent: svgRoot, root, name: 'Hello,'};
  const inline1 = {role: 'inlineTextBox', parent: text1, root, name: 'Hello,'};
  const text2 = {role: 'staticText', parent: svgRoot, root, name: 'world!'};
  const inline2 = {role: 'inlineTextBox', parent: text2, root, name: 'world!'};

  const result = ParagraphUtils.buildNodeGroup(
      [inline1, inline2], 0, false /* do not split on language */);
  assertEquals('Hello, world! ', result.text);
});
