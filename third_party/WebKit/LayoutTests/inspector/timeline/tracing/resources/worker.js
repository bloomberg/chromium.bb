onmessage = function(event) {
    doWork();
    setInterval(doWork, 0);
};
var message_id = 0;
function doWork()
{
    postMessage("Message #" + message_id++);
}
