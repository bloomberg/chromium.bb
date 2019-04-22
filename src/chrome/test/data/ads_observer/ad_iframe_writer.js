// Creates and iframe and appends it to the body element. Make sure the caller
// has a body element!
function createAdIframe() {
    let frame = document.createElement('iframe');
    document.body.appendChild(frame);
    return frame;
}