var isDone = false;

function createCanvas4kBy4k(canvas_id) {
    var myCanvas = document.createElement("canvas");
    myCanvas.id = canvas_id;
    myCanvas.width = 4000;
    myCanvas.height = 4000;
    myCanvas.getContext("2d").fillStyle = 'green';
    myCanvas.getContext("2d").fillRect(0, 0, myCanvas.width, myCanvas.height);
    return myCanvas;
}

function invokeToBlobPng(myCanvas) {
    var startTime = PerfTestRunner.now();
    myCanvas.toBlob(function(blob){
        PerfTestRunner.measureValueAsync(PerfTestRunner.now() - startTime);
        if (!isDone) {
            PerfTestRunner.gc();
            runTest(myCanvas);
        }
    });
}

function draw() {
    if (!isDone)
        requestAnimationFrame(draw);
}

function runTest(myCanvas) {
    draw(); //repeatedly draw the frame to keep main thread busy
    invokeToBlobPng(myCanvas);
}
