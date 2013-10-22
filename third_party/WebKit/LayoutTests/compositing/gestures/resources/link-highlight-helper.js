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

  var offset = _getAbsoluteOffset(node);
  var clientRect = node.getBoundingClientRect();
  div.style.top = offset.top + "px";
  div.style.left = offset.left + "px";
  div.style.width = node.offsetWidth + "px";
  div.style.height = node.offsetHeight + "px";

  return div;
}

function _getAbsoluteOffset( elem )
{
    var offsetLeft = 0;
    var offsetTop = 0;
    do {
      if ( !isNaN( elem.offsetLeft ) )
          offsetLeft += elem.offsetLeft - elem.scrollLeft;

      if ( !isNaN( elem.offsetTop ) )
          offsetTop += elem.offsetTop - elem.scrollTop;

    } while( elem = elem.offsetParent );
    return { top : offsetTop,
             left : offsetLeft };
}
