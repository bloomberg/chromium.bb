if (self.importScripts) {
    importScripts('../../resources/js-test.js');
    importScripts('common.js');
}

function shouldEvaluateAsSilent(expressionToEval, expectedResult)
{
    var result = eval(expressionToEval);
    if (result !== expectedResult) {
        testFailed(expressionToEval + " evaluated to " + result + " instead of " + expectedResult);
    }
}

function doPostMessage(data)
{
    if (isWorker())
        self.postMessage(data);
    else
        self.postMessage(data, '*');
}

function notifySuccess()
{
    doPostMessage("TEST_FINISHED");
}

function notifyFailure(details)
{
    doPostMessage("FAIL:" + details);
}

function testGenerateRsaKey()
{
    var extractable = false;
    var usages = ['encrypt', 'decrypt'];
    // Note that the modulus length is unsually small, in order to speed up the test.
    var algorithm = {name: "RSAES-PKCS1-v1_5", modulusLength: 256, publicExponent: hexStringToUint8Array("010001")};

    return crypto.subtle.generateKey(algorithm, extractable, usages).then(function(result) {
        publicKey = result.publicKey;
        privateKey = result.privateKey;

        shouldEvaluateAsSilent("publicKey.type", "public");
        shouldEvaluateAsSilent("publicKey.extractable", true);
        shouldEvaluateAsSilent("publicKey.algorithm.name", algorithm.name);
        shouldEvaluateAsSilent("publicKey.algorithm.modulusLength", algorithm.modulusLength);
        shouldEvaluateAsSilent("bytesToHexString(publicKey.algorithm.publicExponent)", "010001");

        shouldEvaluateAsSilent("privateKey.type", "private");
        shouldEvaluateAsSilent("privateKey.extractable", false);
        shouldEvaluateAsSilent("privateKey.algorithm.name", algorithm.name);
        shouldEvaluateAsSilent("privateKey.algorithm.modulusLength", algorithm.modulusLength);
        shouldEvaluateAsSilent("bytesToHexString(privateKey.algorithm.publicExponent)", "010001");
    });
}

// Very similar to "hmac-sign-verify.html".
function testHmac()
{
    var importAlgorithm = {name: 'HMAC', hash: {name: "SHA-256"}};
    var algorithm = {name: 'HMAC'};

    var key = null;

    var testCase = {
      hash: "SHA-256",
      key: "9779d9120642797f1747025d5b22b7ac607cab08e1758f2f3a46c8be1e25c53b8c6a8f58ffefa176",
      message: "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e",
      mac: "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b"
    };

    var keyData = hexStringToUint8Array(testCase.key);
    var usages = ['sign', 'verify'];
    var extractable = true;

    // (1) Import the key
    return crypto.subtle.importKey('raw', keyData, importAlgorithm, extractable, usages).then(function(result) {
        key = result;

        // shouldBe() can only resolve variables in global context.
        tmpKey = key;
        shouldEvaluateAsSilent("tmpKey.type", "secret");
        shouldEvaluateAsSilent("tmpKey.extractable", true);
        shouldEvaluateAsSilent("tmpKey.algorithm.name", "HMAC");
        shouldEvaluateAsSilent("tmpKey.algorithm.hash.name", testCase.hash);
        shouldEvaluateAsSilent("tmpKey.algorithm.length", keyData.length * 8);
        shouldEvaluateAsSilent("tmpKey.usages.join(',')", "sign,verify");

        // (2) Sign.
        var signPromise = crypto.subtle.sign(algorithm, key, hexStringToUint8Array(testCase.message));

        // (3) Verify
        var verifyPromise = crypto.subtle.verify(algorithm, key, hexStringToUint8Array(testCase.mac), hexStringToUint8Array(testCase.message));

        // (4) Verify truncated mac (by stripping 1 byte off of it).
        var expectedMac = hexStringToUint8Array(testCase.mac);
        var verifyTruncatedPromise = crypto.subtle.verify(algorithm, key, expectedMac.subarray(0, expectedMac.byteLength - 1), hexStringToUint8Array(testCase.message));

        var exportKeyPromise = crypto.subtle.exportKey('raw', key);

        return Promise.all([signPromise, verifyPromise, verifyTruncatedPromise, exportKeyPromise]);
    }).then(function(result) {
        // signPromise
        mac = result[0];
        shouldEvaluateAsSilent("bytesToHexString(mac)", testCase.mac);

        // verifyPromise
        verifyResult = result[1];
        shouldEvaluateAsSilent("verifyResult", true);

        // verifyTruncatedPromise
        verifyResult = result[2];
        shouldEvaluateAsSilent("verifyResult", false);

        // exportKeyPromise
        exportedKeyData = result[3];
        shouldEvaluateAsSilent("bytesToHexString(exportedKeyData)", testCase.key);
    });
}

// Very similar to aes-gcm-encrypt-decrypt.hml
function testAesGcm()
{
    var testCase = {
      "key": "ae7972c025d7f2ca3dd37dcc3d41c506671765087c6b61b8",
      "iv": "984c1379e6ba961c828d792d",
      "plainText": "d30b02c343487105219d6fa080acc743",
      "cipherText": "c4489fa64a6edf80e7e6a3b8855bc37c",
      "additionalData": "edd8f630f9bbc31b0acf122998f15589d6e6e3e1a3ec89e0c6a6ece751610ebbf57fdfb9d82028ff1d9faebe37a268c1",
      "authenticationTag": "772ee7de0f91a981c36c93a35c88"
    };

    var key = null;
    var keyData = hexStringToUint8Array(testCase.key);
    var iv = hexStringToUint8Array(testCase.iv);
    var additionalData = hexStringToUint8Array(testCase.additionalData);
    var tag = hexStringToUint8Array(testCase.authenticationTag);
    var usages = ['encrypt', 'decrypt'];
    var extractable = false;

    var tagLengthBits = tag.byteLength * 8;

    var algorithm = {name: 'aes-gcm', iv: iv, additionalData: additionalData, tagLength: tagLengthBits};

    // (1) Import the key
    return crypto.subtle.importKey('raw', keyData, algorithm, extractable, usages).then(function(result) {
        key = result;

        // shouldBe() can only resolve variables in global context.
        tmpKey = key;
        shouldEvaluateAsSilent("tmpKey.type", "secret");
        shouldEvaluateAsSilent("tmpKey.extractable", false);
        shouldEvaluateAsSilent("tmpKey.algorithm.name", "AES-GCM");
        shouldEvaluateAsSilent("tmpKey.usages.join(',')", "encrypt,decrypt");

        // (2) Encrypt
        var encryptPromise1 = crypto.subtle.encrypt(algorithm, key, hexStringToUint8Array(testCase.plainText));
        var encryptPromise2 = crypto.subtle.encrypt(algorithm, key, hexStringToUint8Array(testCase.plainText));

        // (3) Decrypt
        var decryptPromise1 = crypto.subtle.decrypt(algorithm, key, hexStringToUint8Array(testCase.cipherText + testCase.authenticationTag));
        var decryptPromise2 = crypto.subtle.decrypt(algorithm, key, hexStringToUint8Array(testCase.cipherText + testCase.authenticationTag));

        return Promise.all([encryptPromise1, encryptPromise2, decryptPromise1, decryptPromise2]);
    }).then(function(result) {
        // encryptPromise1, encryptPromise2
        for (var i = 0; i < 2; ++i) {
            cipherText = result[i];
            shouldEvaluateAsSilent("bytesToHexString(cipherText)", testCase.cipherText + testCase.authenticationTag);
        }

        // decryptPromise1, decryptPromise2
        for (var i = 0; i < 2; ++i) {
            plainText = result[2 + i];
            shouldEvaluateAsSilent("bytesToHexString(plainText)", testCase.plainText);
        }
    });
}

Promise.all([
    testHmac(),
    testGenerateRsaKey(),
    testAesGcm(),
    testHmac(),
    testAesGcm(),
]).then(notifySuccess, notifyFailure);
