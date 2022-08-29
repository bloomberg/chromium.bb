struct VSIn {
    @builtin(vertex_index) sk_VertexID: u32,
};
struct VSOut {
    @location(1) @interpolate(flat) id: i32,
    @builtin(position) sk_Position: vec4<f32>,
};
fn main(_stageIn: VSIn, _stageOut: ptr<function, VSOut>) {
    (*_stageOut).id = i32(_stageIn.sk_VertexID);
}
@stage(vertex) fn vertexMain(_stageIn: VSIn) -> VSOut {
    var _stageOut: VSOut;
    main(_stageIn, &_stageOut);
    return _stageOut;
}
