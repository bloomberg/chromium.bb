// Creates a frame and invokes f when the frame is ready.
window.withFrame = function (f) {
    var frame = document.createElement('iframe');
    frame.onload = function () {
        f(frame);
    };
    frame.src = 'data:text/html,';
    document.body.appendChild(frame);
};
