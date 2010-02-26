uniform sampler2D sampler2d;

varying float v_Dot;
varying vec2 v_texCoord;

void main()
{
    vec2 texCoord = vec2(v_texCoord.s, 1.0 - v_texCoord.t);
    vec4 color = texture2D(sampler2d, texCoord);
    color += vec4(0.1, 0.1, 0.1, 1);
    gl_FragColor = vec4(color.xyz * v_Dot, color.a);
}
