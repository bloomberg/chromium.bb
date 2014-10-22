var message_id = 0;
onmessage = function(event)
{
    postMessage("Ack #" + message_id++);
};
