function createSquareCompositedHighlight(node)
{
  return _createHighlight(node, "squaredHighlight highlightDiv composited");
}

function createCompositedHighlight(node)
{
  return _createHighlight(node, "highlightDiv composited");
}

function createHighlight(node)
{
  return _createHighlight(node, "highlightDiv");
}

function _createHighlight(node, classes) {
  var div = document.createElement('div');
  div.setAttribute('id', 'highlight');
  div.setAttribute('class', classes);
  document.body.appendChild(div);

  var clientRect = node.getBoundingClientRect();
  div.style.top = clientRect.top + "px";
  div.style.left = clientRect.left + "px";
  div.style.width = clientRect.width + "px";
  div.style.height = clientRect.height + "px";

  return div;
}

function useMockHighlight() {
  if (window.internals)
    internals.settings.setMockGestureTapHighlightsEnabled(true);
}

