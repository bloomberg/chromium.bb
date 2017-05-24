// Logs the size of the geometry to the console.
// Fills the context with a green square.

registerPaint('logging-green', class {
    paint(ctx, geom) {
        console.log('width: ' + geom.width + ', height: ' + geom.height);
        ctx.fillStyle = 'green';
        ctx.fillRect(0, 0, geom.width, geom.height);
    }
});
