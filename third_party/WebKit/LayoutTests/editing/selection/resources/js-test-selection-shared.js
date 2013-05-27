function $(id) { return document.getElementById(id); }

var selection= window.getSelection();

function testCaretMotion(param) {
    var direction = param.direction;
    var granularity = param.granularity;
    var repeatCount = param.repeatCount || 1;
    var origin = param.origin;
    var originOffset = param.originOffset || 0;
    var target = param.target;
    var targetOffset = param.targetOffset || 0;

    if (direction == 'both') {
        testCaretMotion({
            'direction': 'forward',
            'granularity': granularity,
            'repeatCount': repeatCount,
            'origin': origin,
            'originOffset': originOffset,
            'target': target,
            'targetOffset': targetOffset
        });
        testCaretMotion({
            'direction': 'backward',
            'granularity': granularity,
            'repeatCount': repeatCount,
            'origin': target,
            'originOffset': targetOffset,
            'target': origin,
            'targetOffset': originOffset
        });
        return;
    }

    if (typeof origin == 'string')
        origin = $(origin);

    if (typeof target == 'string')
        target = $(target);

    if (originOffset < 0)
        originOffset = origin.firstChild.length + originOffset + 1;

    if (targetOffset < 0)
        targetOffset = target.firstChild.length + targetOffset + 1;

    selection.setPosition(origin.firstChild, originOffset);
    debug('Move ' + direction + ' ' + granularity + ' x ' + repeatCount);
    for (var i = 0; i < repeatCount; ++i)
        selection.modify("move", direction, granularity);
    shouldEvaluateTo('selection.anchorNode', target.firstChild);
    shouldEvaluateTo('selection.anchorOffset', targetOffset);
}

function objectAsString(object, properties) {
    var result = "[";
    for (var x = 0; x < properties.length; x++) {
        var property = properties[x];
        if (x != 0)
            result += " ";

        var value = object[property];
        if (value && value.nodeType) // textNode
            value = value + "(" + value.nodeValue + ")";

        result += property + ": " + value;
    }
    result += ']';
    return result;
}

function selectionAsString(sel)
{
    var properties = ['anchorNode', 'anchorOffset', 'focusNode', 'focusOffset', 'isCollapsed'];
    return objectAsString(sel, properties);
}

function assertSelectionAt(anchorNode, anchorOffset, optFocusNode, optFocusOffset) {
    var focusNode = optFocusNode || anchorNode;
    var focusOffset = (optFocusOffset === undefined) ? anchorOffset : optFocusOffset;

    var sel = window.getSelection();
    if (sel.anchorNode == anchorNode
        && sel.focusNode == focusNode
        && sel.anchorOffset == anchorOffset
        && sel.focusOffset == focusOffset) {
        testPassed("Selection is " + selectionAsString(sel));
    } else {
        testFailed("Selection is " + selectionAsString(sel) + 
            " should be at anchorNode: " + anchorNode + " anchorOffset: " + anchorOffset +
            " focusNode: " + focusNode + " focusOffset: " + focusOffset);
    }
}

function clickAt(x, y) {
    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
        return true;
    }
}
