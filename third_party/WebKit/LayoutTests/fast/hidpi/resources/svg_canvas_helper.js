function printSizes(imageId, outputId) {
  var image = document.getElementById(imageId);
  image.addEventListener('load', function() {
    var output = document.getElementById(outputId);
    output.innerHTML = 'image stats: '
        + 'size(' + image.width + ',' + image.height + '), '
        + 'naturalSize(' + image.naturalWidth + ',' + image.naturalHeight + ')<br/>'
        + 'image codes: '
        + image.outerHTML.replace(/</g, "&lt;").replace(/>/g, "&gt;");
  });
}

function drawCanvas(imageId, canvasId, canvasWidth, canvasHeight) {
  var image = document.getElementById(imageId);
  var canvas = document.getElementById(canvasId);
  canvas.width = canvasWidth;
  canvas.height = canvasHeight;
  var context = canvas.getContext('2d');
  context.drawImage(image, 0, 0);
}
