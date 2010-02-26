/*
  Copyright (c) 2008 Seneca College
  Licenced under the MIT License (http://www.c3dl.org/index.php/mit-license/)
*/

// We need to create our own light structure since we can't access 
// the light() function in the 2.0 context.
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

//
// vertex attributes
//
attribute vec3 a_normal; 
attribute vec4 a_texCoord; 
attribute vec4 a_vertex; 

// for every model we multiply the projection, view and model matrices
// once to prevent having to do it for every vertex, however we still need
// the view matrix to calculate lighting.
uniform mat4 u_modelViewMatrix;

// we can calculate this once per model to speed up processing done on the js side.
uniform mat4 u_modelViewProjMatrix;

// matrix to transform the vertex normals
uniform mat4 u_normalMatrix;

// custom light structures need to be used since we don't have access to 
// light states.
uniform Light u_light;

// material
uniform vec4 u_globalAmbientColor;
uniform Material u_frontMaterial;
uniform Material u_backMaterial;

// passed to fragment shader
varying vec4 v_diffuse, v_specular;
varying vec2 v_texCoord;

/*
    Given a reference to the ambient and diffuse  lighting variables,
    this function will calculate how much each component contributes to the scene.

    Light light - the light in view space
    vec3 normal - 
    vec4 ambient - 
    vec4 diffuse - 
*/
void directionalLight(in vec3 normal, inout vec4 ambient, inout vec4 diffuse, inout vec4 specular)
{
    vec3 lightDir = normalize(vec3(u_light.position));
    ambient += u_light.ambient;

    float nDotL = max(dot(normal, lightDir), 0.0);
    if (nDotL > 0.0) {
        diffuse += u_light.diffuse * nDotL;
        float nDotHV = max(dot(normal, u_light.halfVector), 0.0);
        nDotHV += 0.3;
        vec4 specularColor = u_frontMaterial.specular * u_light.specular;
        specular += vec4(specularColor.rgb * pow(nDotHV, u_frontMaterial.shininess), specularColor.a);
    }
}

void main()
{ 
    vec3 normal = normalize(u_normalMatrix * vec4(a_normal, 1)).xyz; 

    vec4 ambient = vec4(0.0, 0.0, 0.0, 1.0); 
    vec4 diffuse = vec4(0.0, 0.0, 0.0, 1.0); 
    vec4 specular = vec4(0.0, 0.0, 0.0, 1.0); 

    // place the current vertex into view space
    // ecPos = eye coordinate position.
    vec4 ecPos4 = u_modelViewMatrix * a_vertex;

    // the current vertex in eye coordinate space
    vec3 ecPos = ecPos4.xyz/ecPos4.w;
    directionalLight(normal, ambient, diffuse, specular);

    ambient = u_frontMaterial.ambient * ambient;
    ambient += u_globalAmbientColor * u_frontMaterial.ambient;
    diffuse = u_frontMaterial.diffuse * diffuse;

    v_diffuse = diffuse;
    v_specular = specular + ambient;
    gl_Position =  u_modelViewProjMatrix * a_vertex;
    v_texCoord = a_texCoord.st;
}
