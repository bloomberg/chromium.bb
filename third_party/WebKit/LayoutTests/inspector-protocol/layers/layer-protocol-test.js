function initialize_layersTest()
{

var layers;
var layerTreeChangeCallback;

InspectorTest.step = function(test)
{
    InspectorTest.sendCommand(test.command, test.parameters, function(messageObject) {
        if (messageObject.hasOwnProperty("error")) {
            InspectorTest.log("FAIL: " + messageObject.error.message + " (" + messageObject.error.code + ")");
            InspectorTest.completeTest();
            return;
        }
        if (test.callback)
            test.callback(messageObject.result);
    });
};

function onLayerTreeChanged(message)
{
    layers = message.params.layers;
    if (layerTreeChangeCallback) {
        var callback = layerTreeChangeCallback;
        layerTreeChangeCallback = null;
        callback(layers);
    }
}

InspectorTest.setLayerTreeChangeCallback = function(callback)
{
    layerTreeChangeCallback = callback;
}

InspectorTest.enableLayerTreeAgent = function(callback)
{
    if (layers) {
        callback(layers);
        return;
    }
    InspectorTest.eventHandler["LayerTree.layerTreeDidChange"] = onLayerTreeChanged;
    InspectorTest.setLayerTreeChangeCallback(callback);
    InspectorTest.sendCommand("DOM.getDocument", {}, function() {
        InspectorTest.sendCommand("LayerTree.enable", {}, function() { });
    });
}

}
