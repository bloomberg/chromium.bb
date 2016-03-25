function nodeOfWordBreak(nodeAndOffset)
{
    var node = document.getElementById(nodeAndOffset[0]).firstChild;
    if (nodeAndOffset.length == 3) {
        var childIndex = nodeAndOffset[2];
        for (var i = 0; i < childIndex - 1; ++i) {
            node = node.nextSibling;
        }
    }
    return node;
}

function positionEqualToWordBreak(position, wordBreak)
{
    if (wordBreak.search(',') == -1)
        return position.offset == wordBreak;
    else {
        var nodeAndOffset = wordBreak.split(',');
        return position.node == nodeOfWordBreak(nodeAndOffset) && position.offset == nodeAndOffset[1];
    }
}

function validateData(positions, wordBreaks)
{
    assert_equals(positions.length, wordBreaks.length, 'wordBreaks.length');
    for (var i = 0; i < wordBreaks.length - 1; ++i)
        assert_true(positionEqualToWordBreak(positions[i], wordBreaks[i]), 'positionEqualToWordBreak of ' + i);
}

const TEST_FOR_CRASH = 'test_for_crash';

function collectWordBreaks(test, searchDirection)
{
    if (test.title == TEST_FOR_CRASH)
        return TEST_FOR_CRASH;

    var title;
    if (searchDirection == 'right')
        title = test.title.split('|')[0];
    else
        title = test.title.split('|')[1];

    var pattern = /\[(.+?)\]/g;
    var result;
    var wordBreaks = [];
    while ((result = pattern.exec(title)) != null) {
        wordBreaks.push(result[1]);
    }
    if (wordBreaks.length == 0) {
        wordBreaks = title.split(' ');
    }
    return wordBreaks;
}

function moveByWord(sel, test, searchDirection, dir)
{
    var prevOffset = sel.anchorOffset;
    var prevNode = sel.anchorNode;
    var positions = [];
    positions.push({ node: sel.anchorNode, offset: sel.anchorOffset });

    while (1) {
        sel.modify('move', searchDirection, 'word');
        if (prevNode == sel.anchorNode && prevOffset == sel.anchorOffset)
            break;
        positions.push({ node: sel.anchorNode, offset: sel.anchorOffset });
        prevNode = sel.anchorNode;
        prevOffset = sel.anchorOffset;
    };

    var wordBreaks = collectWordBreaks(test, searchDirection);
    if (wordBreaks == TEST_FOR_CRASH)
        return;
    validateData(positions, wordBreaks);
}

function moveByWordOnEveryChar(sel, test, searchDirection, dir)
{
    var wordBreaks = collectWordBreaks(test, searchDirection);
    var wordBreakIndex = 1;
    var prevOffset = sel.anchorOffset;
    var prevNode = sel.anchorNode;

    while (1) {
        sel.modify('move', searchDirection, 'word');

        if (wordBreaks != TEST_FOR_CRASH) {
            if (wordBreakIndex >= wordBreaks.length) {
                assert_equals(sel.anchorNode, prevNode, 'expected to stay in the same position');
                assert_equals(sel.anchorOffset, prevOffset, 'expected to stay in the same position');
            } else {
                assert_true(positionEqualToWordBreak({ node: sel.anchorNode, offset: sel.anchorOffset }, wordBreaks[wordBreakIndex]), 'positionEqualToWordBreak of ' + wordBreakIndex);
            }
        }

        // Restore position and move by 1 character.
        sel.collapse(prevNode, prevOffset);
        sel.modify('move', searchDirection, 'character');
        if (prevNode == sel.anchorNode && prevOffset == sel.anchorOffset)
            return;

        if (wordBreakIndex < wordBreaks.length &&
            positionEqualToWordBreak({ node: sel.anchorNode, offset: sel.anchorOffset }, wordBreaks[wordBreakIndex])) {
            ++wordBreakIndex;
        }

        prevNode = sel.anchorNode;
        prevOffset = sel.anchorOffset;
    };
}

function moveByWordForEveryPosition(sel, test, dir)
{
    // Check ctrl-right-arrow works for every position.
    sel.collapse(test, 0);
    var direction = 'right';
    if (dir == 'rtl')
        direction = 'left';
    moveByWord(sel, test, direction, dir);
    sel.collapse(test, 0);
    moveByWordOnEveryChar(sel, test, direction, dir);

    sel.modify('move', 'forward', 'lineBoundary');
    var position = { node: sel.anchorNode, offset: sel.anchorOffset };

    // Check ctrl-left-arrow works for every position.
    if (dir == 'ltr')
        direction = 'left';
    else
        direction = 'right';
    moveByWord(sel, test, direction, dir);

    sel.collapse(position.node, position.offset);
    moveByWordOnEveryChar(sel, test, direction, dir);
}

function runTest() {
  div = document.createElement('div');
  div.id = 'log';
  document.body.appendChild(div);

  var tests = document.getElementsByClassName('test_move_by_word');
  var sel = getSelection();
  for (var testcase of tests) {
      test(function () {
          if (testcase.className.search('fix_width') != -1) {
              var span = document.getElementById('span_size');
              var length = span.offsetWidth;
              testcase.style.width = length + 'px';
          }

          if (testcase.getAttribute('dir') == 'ltr')
              moveByWordForEveryPosition(sel, testcase, 'ltr');
          else
              moveByWordForEveryPosition(sel, testcase, 'rtl');
      }, testcase.id);
  }
}
