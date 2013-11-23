onmessage = function(event)
{
    var count = event.data.count;
    var chunkSize = 1;
    var chunk = new Uint8Array(chunkSize);
    for (var j = 0; j < chunkSize; j++)
        chunk[j] = i + j;
    var blob = new Blob([chunk]);
    for (var i = 0; i < count; i++) {
        var reader = new FileReader();
        reader.readAsArrayBuffer(blob);
    }
}
