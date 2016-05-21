self.onmessage = function(event)
{
    console.log(event.data);
    self.postMessage(event.data);
}
self.postMessage("ready");
