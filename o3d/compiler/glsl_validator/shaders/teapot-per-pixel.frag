struct Light 
{ 
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec4 position;

    vec3 halfVector;
};

struct Material
{
    vec4 emission;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};

uniform sampler2D u_sampler2d;
uniform Light u_light;
uniform Material u_frontMaterial;
uniform Material u_backMaterial;
        
varying vec4 v_diffuse, v_ambient;
varying vec3 v_normal, v_lightDir;
varying vec2 v_texCoord;

void main()
{
    vec4 color = v_diffuse;

    vec3 n = normalize(v_normal);

    Light light = u_light;
    vec3 lightDir = v_lightDir;
    float nDotL = max(dot(n, lightDir), 0.0);
    if (nDotL > 0.0) {
        color = vec4(color.rgb * nDotL, color.a);
        float nDotHV = max(dot(n, light.halfVector), 0.0);
        vec4 specular = u_frontMaterial.specular * light.specular;
        color += vec4(specular.rgb * pow(nDotHV, u_frontMaterial.shininess), specular.a);
    }
            
    gl_FragColor = color + v_ambient;
}
