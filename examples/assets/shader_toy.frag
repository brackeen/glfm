#version 100

precision highp float;

uniform vec3 iResolution;
uniform float iTime;

void mainImage(inout vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);

    fragColor = vec4(0.1 / abs(2.0 * length(uv) - 1.0) + sin(iTime) / 4.0);
}

void main() {
    vec4 fragColor = vec4(0, 0, 0, 1);
    mainImage(fragColor, gl_FragCoord.xy);
    gl_FragColor = fragColor;
}
