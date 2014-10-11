var autoscrollInterval = 50;
var middleButton = 1;
var panScrollRadius = 15; // from FrameView::noPanScrollRadius

window.jsTestIsAsync = true;

function $(id)
{
    return document.getElementById(id);
}

function testPanScroll(param)
{
    function finishTest()
    {
        if ($('container'))
            $('container').innerHTML = '';
        if (param.finishTest)
            param.finishTest();
        if (window.finishJSTest) {
            finishJSTest();
            return;
        }
        if (window.testRunner)
            testRunner.notifyDone();
    }

    var scrollable = param.scrollable;
    var scrolledObject = param.scrolledObject || scrollable;

    if (!scrollable.innerHTML) {
        for (var i = 0; i < 100; ++i) {
            var line = document.createElement('div');
            line.innerHTML = "line " + i;
            scrollable.appendChild(line);
        }
    }

    var noModeScroll = false;
    var scrolled = false;

    scrolledObject.onscroll = function() {
        if (noModeScroll) {
            testFailed('still autoscroll');
            finishTest();
            return;
        }

        if (scrolled)
            return;
        scrolled = true;
        testPassed('autoscroll started');

        if (window.eventSender) {
            if (param.clickOrDrag == 'click')
                eventSender.mouseDown(middleButton);
            eventSender.mouseUp(middleButton);
        }
    };

    scrollable.ownerDocument.onmouseup = function(e) {
        if (!scrolled || e.button != middleButton)
            return;
        noMoreScroll = true;
        window.setTimeout(function() {
            testPassed('autoscroll stopped');
            finishTest();
        }, autoscrollInterval * 2);
    };

    if (!window.eventSender)
        return;
    var startX = param.startX || scrollable.offsetLeft + 5;
    var startY = param.startY || scrollable.offsetTop + 5;
    var endX = param.endX || scrollable.offsetLeft + 5;
    var endY = param.endY || scrollable.offsetTop + panScrollRadius + 6;
    eventSender.mouseMoveTo(startX, startY);
    eventSender.mouseDown(middleButton);
    if (param.clickOrDrag == 'click')
        eventSender.mouseUp(middleButton);
    eventSender.mouseMoveTo(endX, endY);
}
