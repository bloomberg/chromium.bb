function $(id) { return document.getElementById(id); }

var lastScrollLeft;
var lastScrollTop;
window.onload = function() {
    var draggable = $('draggable');
    var sample = $('sample');
    var scrollBarWidth = $('scrollbars').offsetWidth - $('scrollbars').firstChild.offsetWidth;
    var scrollBarHeight = $('scrollbars').offsetHeight - $('scrollbars').firstChild.offsetHeight;
    var eastX = window.innerWidth - scrollBarWidth - 10;
    var northY = 10;
    var southY= window.innerHeight - scrollBarHeight - 10;
    var westX = 10;

    function moveTo(newState, x, y)
    {
        state = newState;
        lastScrollLeft = document.scrollingElement.scrollLeft;
        lastScrollTop = document.scrollingElement.scrollTop;
        eventSender.mouseMoveTo(x, y);
    }

    var state = 'START';
    function process(event)
    {
        debug('state=' + state);
        switch (state) {
        case 'NE':
            shouldBeTrue('document.scrollingElement.scrollLeft > 0');
            shouldBeTrue('!document.scrollingElement.scrollTop');
            moveTo('SE', westX, southY);
            break;
        case 'SE':
            shouldBeTrue('document.scrollingElement.scrollLeft > 0');
            shouldBeTrue('document.scrollingElement.scrollTop > 0');
            moveTo('SW', westX, southY);
            break;
        case 'SW':
            shouldBeTrue('document.scrollingElement.scrollLeft < lastScrollLeft');
            shouldBeTrue('document.scrollingElement.scrollTop > 0');
            moveTo('NW', westX, northY);
            break;
        case 'NW':
            shouldBeTrue('document.scrollingElement.scrollLeft <= lastScrollLeft');
            shouldBeTrue('document.scrollingElement.scrollTop < lastScrollTop');
            eventSender.mouseUp();
            $('container').outerHTML = '';
            if (window.parent !== window)
                window.jsTestIsAsync = false;
            finishJSTest();
            if (!window.jsTestIsAsync)
                window.parent.postMessage($('console').innerHTML, '*');
            state = 'DONE';
            break;
        case 'DONE':
            break;
        case 'START':
            moveTo('NE', eastX, northY);
            break;
        default:
            testFailed('Bad state ' + state);
            break;
        }
    };

    if (!window.eventSender)
        return;

    eventSender.dragMode = false;

    // Grab draggable
    eventSender.mouseMoveTo(draggable.offsetLeft + 5, draggable.offsetTop + 5);
    eventSender.mouseDown();

    window.onscroll = process;
    process();
};
