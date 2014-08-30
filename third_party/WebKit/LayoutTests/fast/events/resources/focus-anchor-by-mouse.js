window.onload = function() {
    if (window.eventSender) {
        var aElement = document.getElementById('anchor');
        eventSender.mouseMoveTo(aElement.offsetLeft + 2, aElement.offsetTop + 2);
        eventSender.mouseDown();
    }
};
