let blobs = [];
let totalBytes = 0;
let errors = [];

function showManualInput() {
  document.body.innerHTML =
  `
  <h4><b>Manual Input:</b></h4>
  <form>
    Blob size: <input type="text" id="blob_size"><br>
    Number of blobs: <input type="text" id="num_blobs"><br>
    <input type="button" value="Start Benchmark" onclick="getParams();" />
  </form>
  <h4><b>Benchmark Output:</b></h4>
  `;
}

function recordError(message) {
  console.log(message);
  errors.push(message);

  let error = document.createElement('div');
  error.textContent = message;
  document.body.appendChild(error);
}

function createBlob(size) {
  let blob = new Blob([new Uint8Array(size)],
                      {type: 'application/octet-string'});
  blobs.push(blob);
  totalBytes += size;
}

function readBlobAsync(blob) {
  const reader = new FileReader();
  return new Promise(resolve => {
    reader.onerror = reportError;
    reader.onloadend = e => { resolve(reader); };
    reader.readAsArrayBuffer(blob);
  });
}

async function createAndRead(size) {
  let blob = new Blob([new Uint8Array(size)],
                      {type: 'application/octet-string'});
  const reader = await readBlobAsync(blob);
  if (reader.error)
    recordError(`Error reading blob: ${reader.error}`);
  else if (reader.result.byteLength != size)
    recordError('Error reading blob: Blob size does not match.');
}

async function readBlobsSerially() {
  if (blobs.length == 0)
    return;

  let totalReadSize = 0;
  for (let i = 0; i < blobs.length; i++) {
    const reader = await readBlobAsync(blobs[i]);
    if (reader.error) {
      recordError(`Error reading blob ${i}: ${reader.error}`);
      return;
    }
    totalReadSize += reader.result.byteLength;
    if (i == blobs.length - 1 && totalReadSize != totalBytes) {
      recordError('Error reading blob: Total blob sizes do not match, ' +
                  `${totalReadSize} vs ${totalBytes}`);
    }
  }
}

function readBlobsInParallel(callback) {
  if (blobs.length == 0)
    return;

  let totalReadSize = 0;
  let numRead = 0;

  let getReader = e => {
    const reader = new FileReader();
    reader.onerror = reportError;
    reader.onloadend = () => {
      if (reader.error) {
        recordError(`Error reading blob: ${reader.error}`);
      } else {
        totalReadSize += reader.result.byteLength;
        if (++numRead == blobs.length) {
          if (totalReadSize != totalBytes) {
            recordError('Error reading blob: Total blob sizes do not match, ' +
                        `${totalReadSize} vs ${totalBytes}`);
          }
          callback();
        }
      }
    };
    return reader;
  }

  blobs.map(blob => getReader().readAsArrayBuffer(blob));
}

async function blobCreateAndImmediatelyRead(numBlobs, size) {
  let start = performance.now();
  errors = [];

  logToDocumentBody(`Creating and reading ${numBlobs} blobs...`);
  for (let i = 0; i < numBlobs; i++)
    await createAndRead(size);
  logToDocumentBody('Finished.');

  if (errors.length)
    logToDocumentBody('Errors on page: ' + errors.join(', '));
}

async function blobCreateAllThenReadSerially(numBlobs, size) {
  errors = [];

  logToDocumentBody(`Creating ${numBlobs} blobs...`);
  for (let i = 0; i < numBlobs; i++)
    createBlob(size);
  logToDocumentBody('Finished creating.');

  logToDocumentBody(`Reading ${numBlobs} blobs serially...`);
  await readBlobsSerially();
  logToDocumentBody('Finished reading.');

  if (errors.length)
    logToDocumentBody('Errors on page: ' + errors.join(', '));
}

async function blobCreateAllThenReadInParallel(numBlobs, size) {
  errors = [];

  logToDocumentBody(`Creating ${numBlobs} blobs...`);
  for (let i = 0; i < numBlobs; i++)
    createBlob(size);
  logToDocumentBody('Finished creating.');

  logToDocumentBody(`Reading ${numBlobs} blobs in parallel...`);
  await new Promise(readBlobsInParallel);
  logToDocumentBody('Finished reading.');

  if (errors.length)
    logToDocumentBody('Errors on page: ' + errors.join(', '));
}
