#version 440
// Flowing colour fields behind the whole window: three soft light sources
// in the palette's colours drift slowly over the background colour. Cheap:
// a few exp() per pixel, no textures.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float aspect;
    float strength;
    vec4 base;
    vec4 tintA;
    vec4 tintB;
    vec4 tintC;
};

float light(vec2 p, vec2 centre, float radius)
{
    vec2 d = p - centre;
    return exp(-dot(d, d) / (radius * radius));
}

void main()
{
    vec2 p = vec2(qt_TexCoord0.x * aspect, qt_TexCoord0.y);
    float t = time;
    vec2 a = vec2((0.78 + 0.16 * sin(t * 0.21)) * aspect, 0.16 + 0.12 * cos(t * 0.17));
    vec2 b = vec2((0.14 + 0.14 * cos(t * 0.13 + 1.1)) * aspect, 0.82 + 0.10 * sin(t * 0.19));
    vec2 c = vec2((0.50 + 0.28 * sin(t * 0.09 + 2.3)) * aspect, 0.50 + 0.22 * cos(t * 0.12 + 0.6));
    float r = 0.42 * max(1.0, aspect * 0.75);

    vec3 colour = base.rgb;
    colour = mix(colour, tintA.rgb, light(p, a, r) * strength);
    colour = mix(colour, tintB.rgb, light(p, b, r * 0.9) * strength * 0.8);
    colour = mix(colour, tintC.rgb, light(p, c, r * 1.2) * strength * 0.45);
    // A gentle fall-off towards the bottom keeps text readable.
    colour = mix(colour, base.rgb, smoothstep(0.55, 1.0, qt_TexCoord0.y) * 0.55);
    fragColor = vec4(colour, 1.0) * qt_Opacity;
}
