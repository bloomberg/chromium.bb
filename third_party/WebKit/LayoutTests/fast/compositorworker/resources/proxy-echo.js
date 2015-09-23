onmessage = function(msg) {
    var proxy = msg.data;
    if (typeof proxy == "CompositorProxy")
        postMessage({type: "error"});
    else
        postMessage({type: 'response', opacity: proxy.supports('opacity'), transform: proxy.supports('transform')});
}
