function selectRange(startElement, startIndex, endElement, endIndex) {
    runAfterLayoutAndPaint(function() {
        if (window.internals)
            window.internals.setSelectionPaintingWithoutSelectionGapsEnabled(true);
        var range = document.createRange();
        range.setStart(startElement, startIndex);
        range.setEnd(endElement, endIndex);
        window.getSelection().addRange(range);
      }, true);
}
