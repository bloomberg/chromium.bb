'use strict';

function createFrame(url, name) {
  let frame = document.createElement('iframe');
  frame.name = name;
  frame.src = url;
  document.body.appendChild(frame);
}

function windowOpenFromNonAdScript() {
  window.open();
}

async function createDocWrittenFrame(name, base_url) {
  let doc_body = await fetch('frame_factory.html');
  let doc_text = await doc_body.text();

  let frame = document.createElement('iframe');
  frame.name = name;
  document.body.appendChild(frame);

  frame.contentDocument.open();
  frame.contentDocument.write(doc_text);
  frame.contentDocument.close();
}
