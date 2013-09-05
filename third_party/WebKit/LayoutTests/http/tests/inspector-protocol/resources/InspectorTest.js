/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
InspectorFrontendAPI = {};

InspectorTest = {};
InspectorTest._dispatchTable = [];
InspectorTest._requestId = -1;
InspectorTest.eventHandler = {};

/**
 * @param {string} method
 * @param {object} params
 * @param {function({object} messageObject)=} handler
 */
InspectorTest.sendCommand = function(method, params, handler)
{
    this._dispatchTable[++this._requestId] = handler;

    var messageObject = { "method": method,
                          "params": params,
                          "id": this._requestId };

    InspectorFrontendHost.sendMessageToBackend(JSON.stringify(messageObject));

    return this._requestId;
}

/**
 * @param {object} messageObject
 */
InspectorFrontendAPI.dispatchMessageAsync = function(messageObject)
{
    var messageId = messageObject["id"];
    if (typeof messageId === "number") {
        var handler = InspectorTest._dispatchTable[messageId];
        if (handler && typeof handler === "function")
            handler(messageObject);
    } else {
        var eventName = messageObject["method"];
        var eventHandler = InspectorTest.eventHandler[eventName];
        if (eventHandler)
            eventHandler(messageObject);
    }
}

/**
* Logs message to document.
* @param {string} message
*/
InspectorTest.log = function(message)
{
    this.sendCommand("Runtime.evaluate", { "expression": "log(" + JSON.stringify(message) + ")" } );
}

/**
* Formats and logs object to document.
* @param {Object} object
* @param {string=} title
*/
InspectorTest.logObject = function(object, title)
{
    var lines = [];

    function dumpValue(value, prefix, prefixWithName)
    {
        if (typeof value === "object" && value !== null) {
            if (value instanceof Array)
                dumpItems(value, prefix, prefixWithName);
            else
                dumpProperties(value, prefix, prefixWithName);
        } else {
            lines.push(prefixWithName + String(value).replace(/\n/g, " "));
        }
    }

    function dumpProperties(object, prefix, firstLinePrefix)
    {
        prefix = prefix || "";
        firstLinePrefix = firstLinePrefix || prefix;
        lines.push(firstLinePrefix + "{");

        var propertyNames = Object.keys(object);
        propertyNames.sort();
        for (var i = 0; i < propertyNames.length; ++i) {
            var name = propertyNames[i];
            if (typeof object.hasOwnProperty === "function" && !object.hasOwnProperty(name))
                continue;
            var prefixWithName = "    " + prefix + name + " : ";
            dumpValue(object[name], "    " + prefix, prefixWithName);
        }
        lines.push(prefix + "}");
    }

    function dumpItems(object, prefix, firstLinePrefix)
    {
        prefix = prefix || "";
        firstLinePrefix = firstLinePrefix || prefix;
        lines.push(firstLinePrefix + "[");
        for (var i = 0; i < object.length; ++i)
            dumpValue(object[i], "    " + prefix, "    " + prefix + "[" + i + "] : ");
        lines.push(prefix + "]");
    }

    dumpValue(object, "", title);
    InspectorTest.log(lines.join("\n"));
}

/**
* Logs message directly to process stdout via alert function (hopefully followed by flush call).
* This message should survive process crash or kill by timeout. 
* @param {string} message
*/
InspectorTest.debugLog = function(message)
{
    this.sendCommand("Runtime.evaluate", { "expression": "debugLog(" + JSON.stringify(message) + ")" } );
}

InspectorTest.completeTest = function()
{
    this.sendCommand("Runtime.evaluate", { "expression": "closeTest();"} );
}

InspectorTest.checkExpectation = function(fail, name, messageObject)
{
    if (fail === !!messageObject.error) {
        InspectorTest.log("PASS: " + name);
        return true;
    }

    InspectorTest.log("FAIL: " + name + ": " + JSON.stringify(messageObject));
    InspectorTest.completeTest();
    return false;
}
InspectorTest.expectedSuccess = InspectorTest.checkExpectation.bind(null, false);
InspectorTest.expectedError = InspectorTest.checkExpectation.bind(null, true);

/**
 * @param {string} scriptName
 */
InspectorTest.importScript = function(scriptName)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", scriptName, false);
    xhr.send(null);
    window.eval(xhr.responseText + "\n//@ sourceURL=" + scriptName);
}

window.addEventListener("message", function(event) {
    try {
        eval(event.data);
    } catch (e) {
        alert(e.stack);
        InspectorTest.completeTest();
        throw e;
    }
});
