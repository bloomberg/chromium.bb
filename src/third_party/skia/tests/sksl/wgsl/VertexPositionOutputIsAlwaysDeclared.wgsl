struct VSOut {
    @builtin(position) sk_Position: vec4<f32>,
};
fn main() {
}
@stage(vertex) fn vertexMain() -> VSOut {
    var _stageOut: VSOut;
    main();
    return _stageOut;
}
