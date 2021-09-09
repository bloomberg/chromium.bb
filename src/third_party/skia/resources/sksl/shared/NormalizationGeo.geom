/*#pragma settings GeometryShaderSupport*/

uniform float4 sk_RTAdjust;
layout(points) in;
layout(invocations = 2) in;
layout(line_strip, max_vertices = 2) out;

void main() {
    sk_Position = sk_in[0].sk_Position + float4(-0.5, 0, 0, sk_InvocationID);
    EmitVertex();
    sk_Position = sk_in[0].sk_Position + float4(0.5, 0, 0, sk_InvocationID);
    EmitVertex();
    EndPrimitive();
}
