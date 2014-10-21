onmessage = function(event) {
    doWork();
    // FIXME: we need a better way of waiting for layout/repainting to happen
    setInterval(doWork, 1);
};
var message_id = 0;
function doWork()
{
    postMessage("Message #" + message_id++);
}
