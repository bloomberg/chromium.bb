(function () {

function isParentFrame() {
    return window.parent === window;
}

var childDoneMessage = 'PASS child done';

fork = function () {
    if (isParentFrame()) {
        window.addEventListener('message', function (event) {
            debug(event.data);
            if (event.data == childDoneMessage)
                finishJSTest();
        });

        var iframe = document.createElement('iframe');
        iframe.src = window.location;
        document.body.appendChild(iframe);
        iframe = null;
    }

    return isParentFrame();
};

if (!isParentFrame()) {
    var parent = window.parent;
    log = function (msg) {
        parent.postMessage(msg, '*');
    };

    done = function () {
        log(childDoneMessage);
    };

    destroyContext = function () {
        parent.document.querySelector('iframe').remove();
        log('PASS destroyed context');
    };
}

})();
