describe('Runtime shader effects', () => {
    let container;

    beforeEach(async () => {
        await LoadCanvasKit;
        container = document.createElement('div');
        container.innerHTML = `
            <canvas width=600 height=600 id=test></canvas>
            <canvas width=600 height=600 id=report></canvas>`;
        document.body.appendChild(container);
    });

    afterEach(() => {
        document.body.removeChild(container);
    });

    // On the SW backend, atan is not supported - a shader is returned, but
    // it will draw blank.
    const spiralSkSL = `
uniform float rad_scale;
uniform float2 in_center;
uniform float4 in_colors0;
uniform float4 in_colors1;

void main(float2 p, inout half4 color) {
    float2 pp = p - in_center;
    float radius = sqrt(dot(pp, pp));
    radius = sqrt(radius);
    float angle = atan(pp.y / pp.x);
    float t = (angle + 3.1415926/2) / (3.1415926);
    t += radius * rad_scale;
    t = fract(t);
    color = half4(mix(in_colors0, in_colors1, t));
}`;

    // TODO(kjlubick) rewrite testRTShader and callers to use gm.
    const testRTShader = (name, done, localMatrix) => {
        const surface = CanvasKit.MakeCanvasSurface('test');
        expect(surface).toBeTruthy('Could not make surface');
        if (!surface) {
            return;
        }
        const spiral = CanvasKit.SkRuntimeEffect.Make(spiralSkSL);
        expect(spiral).toBeTruthy('could not compile program');
        const canvas = surface.getCanvas();
        const paint = new CanvasKit.SkPaint();
        canvas.clear(CanvasKit.BLACK); // black should not be visible
        const shader = spiral.makeShader([
            0.3,
            CANVAS_WIDTH/2, CANVAS_HEIGHT/2,
            1, 0, 0, 1, // solid red
            0, 1, 0, 1], // solid green
            true, /*=opaque*/
            localMatrix);
        paint.setShader(shader);
        canvas.drawRect(CanvasKit.LTRBRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT), paint);

        paint.delete();
        shader.delete();
        spiral.delete();

        reportSurface(surface, name, done);
    };

    it('can compile custom shader code', (done) => {
        testRTShader('rtshader_spiral', done);
    });

    it('can apply a matrix to the shader', (done) => {
        testRTShader('rtshader_spiral_translated', done, CanvasKit.SkMatrix.translated(-200, 100));
    });

    const loadBrick = fetch(
        '/assets/brickwork-texture.jpg')
        .then((response) => response.arrayBuffer());
    const loadMandrill = fetch(
        '/assets/mandrill_512.png')
        .then((response) => response.arrayBuffer());

    const thresholdSkSL = `
in fragmentProcessor before_map;
in fragmentProcessor after_map;
in fragmentProcessor threshold_map;

uniform float cutoff;
uniform float slope;

float smooth_cutoff(float x) {
    x = x * slope + (0.5 - slope * cutoff);
    return clamp(x, 0, 1);
}

void main(float2 xy, inout half4 color) {
    half4 before = sample(before_map, xy);
    half4 after = sample(after_map, xy);

    float m = smooth_cutoff(sample(threshold_map, xy).r);
    color = mix(before, after, half(m));
}`;

    // TODO(kjlubick) rewrite testChildrenShader and callers to use gm.
    const testChildrenShader = (name, done, localMatrix) => {
        Promise.all([loadBrick, loadMandrill]).then((values) => {
            catchException(done, () => {
                const [brickData, mandrillData] = values;
                const brickImg = CanvasKit.MakeImageFromEncoded(brickData);
                expect(brickImg).toBeTruthy('brick image could not be loaded');
                const mandrillImg = CanvasKit.MakeImageFromEncoded(mandrillData);
                expect(mandrillImg).toBeTruthy('mandrill image could not be loaded');

                const thresholdEffect = CanvasKit.SkRuntimeEffect.Make(thresholdSkSL);
                expect(thresholdEffect).toBeTruthy('threshold did not compile');
                const spiralEffect = CanvasKit.SkRuntimeEffect.Make(spiralSkSL);
                expect(spiralEffect).toBeTruthy('spiral did not compile');

                const brickShader = brickImg.makeShader(
                    CanvasKit.TileMode.Decal, CanvasKit.TileMode.Decal,
                    CanvasKit.SkMatrix.scaled(CANVAS_WIDTH/brickImg.width(),
                                              CANVAS_HEIGHT/brickImg.height()));
                const mandrillShader = mandrillImg.makeShader(
                    CanvasKit.TileMode.Decal, CanvasKit.TileMode.Decal,
                    CanvasKit.SkMatrix.scaled(CANVAS_WIDTH/mandrillImg.width(),
                                              CANVAS_HEIGHT/mandrillImg.height()));
                const spiralShader = spiralEffect.makeShader([
                    0.8,
                    CANVAS_WIDTH/2, CANVAS_HEIGHT/2,
                    1, 1, 1, 1,
                    0, 0, 0, 1], true);

                const blendShader = thresholdEffect.makeShaderWithChildren(
                    [0.5, 5],
                    true, [brickShader, mandrillShader, spiralShader], localMatrix);

                const surface = CanvasKit.MakeCanvasSurface('test');
                expect(surface).toBeTruthy('Could not make surface');
                const canvas = surface.getCanvas();
                const paint = new CanvasKit.SkPaint();
                canvas.clear(CanvasKit.WHITE);

                paint.setShader(blendShader);
                canvas.drawRect(CanvasKit.LTRBRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT), paint);

                brickImg.delete();
                mandrillImg.delete();
                thresholdEffect.delete();
                spiralEffect.delete();
                brickShader.delete();
                mandrillShader.delete();
                spiralShader.delete();
                blendShader.delete();
                paint.delete();

                reportSurface(surface, name, done);
            })();
        });
    }

    it('take other shaders as fragment processors', (done) => {
        testChildrenShader('rtshader_children', done);
    });

    it('apply a local matrix to the children-based shader', (done) => {
        testChildrenShader('rtshader_children_rotated', done, CanvasKit.SkMatrix.rotated(Math.PI/12));
    });
});
