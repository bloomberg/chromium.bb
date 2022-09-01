struct FSIn {
    @builtin(position) sk_FragCoord: vec4<f32>,
    @builtin(front_facing) sk_Clockwise: bool,
};
struct FSOut {
    @location(0) sk_FragColor: vec4<f32>,
};
fn main(_stageIn: FSIn) -> vec4<f32> {
    return vec4<f32>(f32(_stageIn.sk_FragCoord.x), 0.0, 0.0, 1.0);
}
@stage(fragment) fn fragmentMain(_stageIn: FSIn) -> FSOut {
    var _stageOut: FSOut;
    _stageOut.sk_FragColor = main(_stageIn);
    return _stageOut;
}
