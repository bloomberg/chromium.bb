function verifyArray(ta, length) {
    for (i = 0; i < length; ++i) {
        if (ta[i] != i) {
            postMessage("FAIL: Transferred data is incorrect. Expected " +
                i + " got " + ta[i]);
            return;
        }
    }
    postMessage("PASS: Transferred data is OK.");
}

function verifyArrayType(ta, name) {
    var className = Object.prototype.toString.call(ta);
    if (className.indexOf(name) != -1)
        postMessage("PASS: Transferred array type is OK.");
    else
        postMessage("FAIL: Expected array type " + name + " got " + className);
}

self.addEventListener('message', function(e) {
    var i;
    var sab;
    var ta;

    switch (e.data.name) {
        case 'SharedArrayBuffer':
            sab = e.data.data;
            ta = new Uint8Array(sab);
            verifyArray(ta, e.data.length);
            break;

        case 'Int8Array':
        case 'Uint8Array':
        case 'Uint8ClampedArray':
        case 'Int16Array':
        case 'Uint16Array':
        case 'Int32Array':
        case 'Uint32Array':
        case 'Float32Array':
        case 'Float64Array':
            ta = e.data.data;
            verifyArrayType(ta, e.data.name);
            verifyArray(ta, e.data.length);
            break;

        default:
            postMessage("ERROR: unknown command " + e.data.name);
            break;
    }
    postMessage("DONE");
});
