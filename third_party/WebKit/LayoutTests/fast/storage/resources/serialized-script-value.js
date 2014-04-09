var kSerializedScriptValueVersion = 6;

function forVersion(version, values) {
    var versionTag = 0xff;
    var versionPrefix = [ (version << 8) | versionTag ];

    return versionPrefix.concat(values)
}

function expectBufferValue(bytesPerElement, expectedValues, buffer) {
    expectedBufferValues = expectedValues;
    var arrayClass;
    if (bytesPerElement == 1)
        arrayClass = Uint8Array;
    else
        arrayClass = Uint16Array;
    bufferView = new arrayClass(buffer);
    shouldBe("bufferView.length", "expectedBufferValues.length");
    var success = (bufferView.length == expectedBufferValues.length);
    if (success) {
        for (var i = 0; i < expectedValues.length; i++) {
            if (expectedValues[i] != bufferView[i]) {
                testFailed("ArrayBufferViews differ at index " + i + ". Should be " + expectedValues[i] + ". Was " + bufferView[i]);
                success = false;
                break;
            }
        }
    }

    if (!success) {
        // output the full buffer for adding back into the test
        var output = [];
        for (i = 0; i < bufferView.length; i++) {
            var hexVal = bufferView[i].toString(16);
            if (hexVal.length < bytesPerElement * 2) {
                hexVal = "0000".slice(hexVal.length, bytesPerElement * 2) + hexVal;
            }
            output.push("0x" + hexVal);
        }
        debug("Actual buffer: [" + output.join(", ") + "]");
    }
}

function makeBuffer(bytesPerElement, serializedValues) {
    var arrayClass;
    if (bytesPerElement == 1)
        arrayClass = Uint8Array;
    else
        arrayClass = Uint16Array;

    var bufferView = new arrayClass(new ArrayBuffer(serializedValues.length * bytesPerElement));
    for (var i = 0; i < serializedValues.length; i++) {
        bufferView[i] = serializedValues[i];
    }
    return bufferView.buffer;
}

function areValuesIdentical(a, b) {
    function sortObject(object) {
        if (typeof object != "object" || object === null)
            return object;
        return Object.keys(object).sort().map(function(key) {
            return { key: key, value: sortObject(object[key]) };
        });
    }
    return JSON.stringify(sortObject(a)) === JSON.stringify(sortObject(b));
}

function _testSerialization(bytesPerElement, obj, values, oldFormat, serializeExceptionValue) {
    debug("");

    values = forVersion(kSerializedScriptValueVersion, values);

    if (!serializeExceptionValue) {
        self.obj = obj;
        debug("Deserialize to " + JSON.stringify(obj) + ":");
        self.newObj = internals.deserializeBuffer(makeBuffer(bytesPerElement, values));
        shouldBe("JSON.stringify(newObj)", "JSON.stringify(obj)");
        shouldBeTrue("areValuesIdentical(newObj, obj)");

        if (oldFormat) {
            self.newObj = internals.deserializeBuffer(makeBuffer(bytesPerElement, oldFormat));
            shouldBe("JSON.stringify(newObj)", "JSON.stringify(obj)");
            shouldBeTrue("areValuesIdentical(newObj, obj)");
        }
    }

    debug("Serialize " + JSON.stringify(obj) + ":");
    try {
        var serialized = internals.serializeObject(obj);
        if (serializeExceptionValue) {
            testFailed("Should have thrown an exception of type ", serializeExceptionValue);
        }
    } catch(e) {
        if (!serializeExceptionValue) {
            testFailed("Threw exception " + e);
            return;
        } else {
            self.thrownException = e;
            self.expectedException = serializeExceptionValue;
            shouldBe("thrownException.code", "expectedException");
            return;
        }
    }
    expectBufferValue(bytesPerElement, values, serialized);
}

function testBlobSerialization() {
    debug("");
    self.blobObj1 = new Blob(['Hi'], {type: 'text/plain'});
    self.blobObj2 = internals.deserializeBuffer(internals.serializeObject(blobObj1));
    shouldBeTrue("areValuesIdentical(blobObj1, blobObj2)");
    self.dictionaryWithBlob1 = {blob: blobObj1, string: 'stringValue'};
    self.dictionaryWithBlob2 = internals.deserializeBuffer(internals.serializeObject(dictionaryWithBlob1));
    shouldBeTrue("areValuesIdentical(dictionaryWithBlob1, dictionaryWithBlob2)");

    // Compare contents too.
    var xhr1 = new XMLHttpRequest();
    xhr1.open("GET", URL.createObjectURL(self.blobObj1), false);
    xhr1.send();
    self.blobContent1 = xhr1.response;
    var xhr2 = new XMLHttpRequest();
    xhr2.open("GET", URL.createObjectURL(self.blobObj2), false);
    xhr2.send();
    self.blobContent2 = xhr2.response;
    shouldBe("self.blobContent1", "self.blobContent2");
}

function testFileSerialization() {
    debug("");
    window.jsTestIsAsync = true;

    function checkIfSameContent(file1, file2, continuation) {
        function step1()  {
            var fileReader = new FileReader();
            fileReader.onload = function () {
                self.fileContents1 = fileReader.result;
                step2();
            };
            fileReader.readAsText(file1);
        }
        function step2() {
            var fileReader = new FileReader();
            fileReader.onload = function () {
                self.fileContents2 = fileReader.result;
                finish();
            };
            fileReader.readAsText(file2);
        }
        function finish() {
            shouldBe("self.fileContents1", "self.fileContents2");
            continuation();
        }
        step1();
    }

    debug("Test deserialization of File objects");
    self.fileObj1 = new File(['The', ' contents'], 'testfile.txt', {type: 'text/plain', lastModified: 0});
    self.fileObj2 = internals.deserializeBuffer(internals.serializeObject(self.fileObj1));
    shouldBeTrue("areValuesIdentical(fileObj1, fileObj2)");
    self.dictionaryWithFile1 = {file: fileObj1, string: 'stringValue'};
    self.dictionaryWithFile2 = internals.deserializeBuffer(internals.serializeObject(dictionaryWithFile1));
    shouldBeTrue("areValuesIdentical(dictionaryWithFile1, dictionaryWithFile2)");

    // Read and compare actual contents.
    checkIfSameContent(self.fileObj1, self.fileObj2, finishJSTest);
}
