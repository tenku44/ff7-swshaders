#pragma once

namespace rh::shaders {

inline constexpr char kFullscreenTriangleVs[] = R"GLSL(#version 320

precision highp float;

out vec2 vUv;

void main()
{
	vUv = vec2((gl_VertexID) & 2, (gl_VertexID << 1) & 2);
	gl_Position = vec4(vUv * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

// Scanlines.
inline constexpr char kScanlinesFs[] = R"GLSL(#version 320

precision highp float;

layout(location = 0, binding = 0) uniform sampler2D Sampler0;

uniform float Strength;   // 0 = no effect, 1 = fully dark gaps
uniform float Lines;      // number of scanlines across the screen height

in vec2 vUv;

layout(location = 0) out vec4 pColor;

void main()
{
	vec3 c = texture(Sampler0, vUv).rgb;
	float line = vUv.y * Lines;
	float s = 0.5 + 0.5 * cos(6.2831853 * (line - 0.5));    // 1 at a line's centre, 0 between lines
	float m = mix(1.0 - Strength, 1.0, s);
	float mean = 1.0 - 0.5 * Strength;                       // keeps the average brightness roughly constant
	pColor = vec4(min(c * (m / mean), vec3(1.0)), 1.0);
}
)GLSL";

inline constexpr char kCrtGeomVs[] = R"GLSL(#version 320

precision highp float;

uniform float d;
uniform float R;
uniform float x_tilt;
uniform float y_tilt;
uniform float SHARPER;
uniform vec2 SourceSize;

#define TextureSize SourceSize
#define InputSize SourceSize

out vec2 vUv;
out vec2 vOne;
out vec2 vIlfac;
out vec3 vStretch;
out vec2 vSin;
out vec2 vCos;

vec2 sinangle;
vec2 cosangle;
vec2 aspect;

#define FIX(c) max(abs(c), 1e-5)

float intersect(vec2 xy)
{
	float A = dot(xy,xy)+d*d;
	float B = 2.0*(R*(dot(xy,sinangle)-d*cosangle.x*cosangle.y)-d*d);
	float C = d*d + 2.0*R*d*cosangle.x*cosangle.y;
	return (-B-sqrt(B*B-4.0*A*C))/(2.0*A);
}

vec2 bkwtrans(vec2 xy)
{
	float c = intersect(xy);
	vec2 point = vec2(c)*xy;
	point -= vec2(-R)*sinangle;
	point /= vec2(R);
	vec2 tang = sinangle/cosangle;
	vec2 poc = point/cosangle;
	float A = dot(tang,tang)+1.0;
	float B = -2.0*dot(poc,tang);
	float C = dot(poc,poc)-1.0;
	float a = (-B+sqrt(B*B-4.0*A*C))/(2.0*A);
	vec2 uv = (point-a*sinangle)/cosangle;
	float r = R*acos(a);
	return uv*r/sin(r/R);
}

vec2 fwtrans(vec2 uv)
{
	float r = FIX(sqrt(dot(uv,uv)));
	uv *= sin(r/R)/r;
	float x = 1.0-cos(r/R);
	float D = d/R + x*cosangle.x*cosangle.y+dot(uv,sinangle);
	return d*(uv*cosangle-x*sinangle)/D;
}

vec3 maxscale()
{
	vec2 c = bkwtrans(-R * sinangle / (1.0 + R/d*cosangle.x*cosangle.y));
	vec2 a = vec2(0.5,0.5)*aspect;
	vec2 lo = vec2(fwtrans(vec2(-a.x,c.y)).x, fwtrans(vec2(c.x,-a.y)).y)/aspect;
	vec2 hi = vec2(fwtrans(vec2(+a.x,c.y)).x, fwtrans(vec2(c.x,+a.y)).y)/aspect;
	return vec3((hi+lo)*aspect*0.5,max(hi.x-lo.x,hi.y-lo.y));
}

void main()
{
	vUv = vec2((gl_VertexID) & 2, (gl_VertexID << 1) & 2);
	gl_Position = vec4(vUv * 2.0 - 1.0, 0.0, 1.0);

	aspect = vec2(1.0, 0.75);
	sinangle = sin(vec2(x_tilt, y_tilt)) + vec2(0.001);
	cosangle = cos(vec2(x_tilt, y_tilt)) + vec2(0.001);
	vStretch = maxscale();

	vIlfac = vec2(1.0, clamp(floor(InputSize.y/200.0), 1.0, 2.0));
	vOne = vIlfac / vec2(SHARPER * TextureSize.x, TextureSize.y);
	vSin = sinangle;
	vCos = cosangle;
}
)GLSL";

inline constexpr char kCrtGeomFs[] = R"GLSL(#version 320

precision highp float;

layout(location = 0, binding = 0) uniform sampler2D Sampler0;

in vec2 vUv;
in vec2 vOne;
in vec2 vIlfac;
in vec3 vStretch;
in vec2 vSin;
in vec2 vCos;

layout(location = 0) out vec4 pColor;

uniform float CRTgamma;
uniform float monitorgamma;
uniform float d;
uniform float CURVATURE;
uniform float R;
uniform float cornersize;
uniform float cornersmooth;
uniform float overscan;
#define overscan_x overscan
#define overscan_y overscan
uniform float DOTMASK;
uniform float scanline_weight;
uniform float LUM;
uniform float SATURATION;
const float INV = 0.0;

uniform vec2 SourceSize;
#define TextureSize SourceSize
#define InputSize SourceSize
const vec2 aspect = vec2(1.0, 0.75);

#define one vOne
#define ilfac vIlfac
#define stretch vStretch
#define sinangle vSin
#define cosangle vCos

#define PI 3.141592653589
#define FIX(c) max(abs(c), 1e-5)
#define TEX2D(c) pow(texture(Sampler0, (c)), vec4(CRTgamma))

float intersect(vec2 xy)
{
	float A = dot(xy,xy)+d*d;
	float B = 2.0*(R*(dot(xy,sinangle)-d*cosangle.x*cosangle.y)-d*d);
	float C = d*d + 2.0*R*d*cosangle.x*cosangle.y;
	return (-B-sqrt(B*B-4.0*A*C))/(2.0*A);
}

vec2 bkwtrans(vec2 xy)
{
	float c = intersect(xy);
	vec2 point = vec2(c)*xy;
	point -= vec2(-R)*sinangle;
	point /= vec2(R);
	vec2 tang = sinangle/cosangle;
	vec2 poc = point/cosangle;
	float A = dot(tang,tang)+1.0;
	float B = -2.0*dot(poc,tang);
	float C = dot(poc,poc)-1.0;
	float a = (-B+sqrt(B*B-4.0*A*C))/(2.0*A);
	vec2 uv = (point-a*sinangle)/cosangle;
	float r = FIX(R*acos(a));
	return uv*r/sin(r/R);
}

vec2 transform(vec2 coord)
{
	coord *= TextureSize / InputSize;
	coord = (coord-vec2(0.5))*aspect*stretch.z+stretch.xy;
	return (bkwtrans(coord)/vec2(overscan_x / 100.0, overscan_y / 100.0)/aspect+vec2(0.5)) * InputSize / TextureSize;
}

float corner(vec2 coord)
{
	coord *= TextureSize / InputSize;
	coord = (coord - vec2(0.5)) * vec2(overscan_x / 100.0, overscan_y / 100.0) + vec2(0.5);
	coord = min(coord, vec2(1.0)-coord) * aspect;
	vec2 cdist = vec2(cornersize);
	coord = (cdist - min(coord,cdist));
	float dist = sqrt(dot(coord,coord));
	return clamp((cdist.x-dist)*cornersmooth,0.0, 1.0)*1.0001;
}

vec4 scanlineWeights(float dist, vec4 color)
{
	vec4 wid = 2.0 + 2.0 * pow(color, vec4(4.0));
	vec4 weights = vec4(dist / scanline_weight);
	return (LUM + 1.4) * exp(-pow(weights * inversesqrt(0.5 * wid), wid)) / (0.6 + 0.2 * wid);
}

vec3 saturation(vec3 textureColor)
{
	float l = length(textureColor)*0.5775;

	vec3 luminanceWeighting = vec3(0.3,0.6,0.1);
	if (l<0.5) luminanceWeighting.rgb=(luminanceWeighting.rgb*luminanceWeighting.rgb)+(luminanceWeighting.rgb*luminanceWeighting.rgb);

	float luminance = dot(textureColor, luminanceWeighting);
	vec3 greyScaleColor = vec3(luminance);

	return vec3(mix(greyScaleColor, textureColor, SATURATION));
}

#define pwr vec3(1.0/((-0.7*(1.0-scanline_weight)+1.0)*(-0.5*DOTMASK+1.0))-1.25)

vec3 inv_gamma(vec3 col, vec3 power)
{
	vec3 cir  = col-1.0;
	     cir *= cir;
	     col  = mix(sqrt(col),sqrt(1.0-cir),power);
	return col;
}

void main()
{
	vec2 OutputSize = vec2(textureSize(Sampler0, 0));
	float mod_factor = vUv.x * OutputSize.x;

	vec2 xy = (CURVATURE > 0.5) ? transform(vUv * 1.0001) : vUv * 1.0001;

	float cval = corner(xy);

	vec2 ilvec = vec2(0.0, 0.0);
	vec2 ratio_scale = (xy * TextureSize - vec2(0.5) + ilvec)/ilfac;
	float filter_ = InputSize.y/OutputSize.y;
	vec2 uv_ratio = fract(ratio_scale);

	xy = (floor(ratio_scale)*ilfac + vec2(0.5) - ilvec) / TextureSize;

	vec4 coeffs = PI * vec4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x);
	coeffs = FIX(coeffs);
	coeffs = 2.0 * sin(coeffs) * sin(coeffs / 2.0) / (coeffs * coeffs);
	coeffs /= dot(coeffs, vec4(1.0));

	vec4 col  = clamp(mat4(
		TEX2D(xy + vec2(-one.x, 0.0)),
		TEX2D(xy),
		TEX2D(xy + vec2(one.x, 0.0)),
		TEX2D(xy + vec2(2.0 * one.x, 0.0))) * coeffs,
		0.0, 1.0);
	vec4 col2 = clamp(mat4(
		TEX2D(xy + vec2(-one.x, one.y)),
		TEX2D(xy + vec2(0.0, one.y)),
		TEX2D(xy + one),
		TEX2D(xy + vec2(2.0 * one.x, one.y))) * coeffs,
		0.0, 1.0);

	vec4 weights  = scanlineWeights(uv_ratio.y, col);
	vec4 weights2 = scanlineWeights(1.0 - uv_ratio.y, col2);

	uv_ratio.y = uv_ratio.y + 1.0/3.0*filter_;
	weights  = (weights + scanlineWeights(uv_ratio.y, col))/3.0;
	weights2 = (weights2 + scanlineWeights(abs(1.0 - uv_ratio.y), col2))/3.0;
	uv_ratio.y = uv_ratio.y - 2.0/3.0*filter_;
	weights  = weights + scanlineWeights(abs(uv_ratio.y), col)/3.0;
	weights2 = weights2 + scanlineWeights(abs(1.0 - uv_ratio.y), col2)/3.0;

	vec3 mul_res = (col * weights + col2 * weights2).rgb * vec3(cval);

	vec3 dotMaskWeights = mix(
		vec3(1.0, 1.0 - DOTMASK, 1.0),
		vec3(1.0 - DOTMASK, 1.0, 1.0 - DOTMASK),
		floor(mod(mod_factor, 2.0))
	);
	mul_res *= dotMaskWeights;

	if (INV == 1.0) { mul_res = inv_gamma(mul_res, pwr); }
	else mul_res = pow(mul_res, vec3(1.0/monitorgamma));

	mul_res = saturation(mul_res);

	pColor = vec4(mul_res, 1.0);
}
)GLSL";

inline constexpr char kCrtEzFs[] = R"GLSL(#version 320
precision highp float;

layout(location = 0, binding = 0) uniform sampler2D Sampler0;

in vec2 vUv;
layout(location = 0) out vec4 pColor;

#define PI 3.141592653589
#define FIX(c) max(abs(c), 1e-5)
#define TEX2D(c) texture(Sampler0, (c))

uniform float SHARPNESS_H;
uniform float SHARPNESS_V;
uniform float MASK_STRENGTH;
uniform float MASK_DOT_WIDTH;
uniform float MASK_DOT_HEIGHT;
uniform float MASK_STAGGER;
uniform float MASK_SIZE;
uniform float SCANLINE_STRENGTH;
uniform float SCANLINE_BEAM_WIDTH_MIN;
uniform float SCANLINE_BEAM_WIDTH_MAX;
uniform float SCANLINE_BRIGHT_MIN;
uniform float SCANLINE_BRIGHT_MAX;
uniform float SCANLINE_CUTOFF;
uniform float GAMMA_INPUT;
uniform float GAMMA_OUTPUT;
uniform float BRIGHT_BOOST;
uniform float DILATION;

uniform vec2 SourceSize;
#define TextureSize SourceSize
#define InputSize SourceSize

#define ENABLE_LANCZOS 1

vec4 dilate(vec4 col)
{
    vec4 x = mix(vec4(1.0), col, DILATION);

    return col * x;
}

float curve_distance(float x, float sharp)
{

/*
    apply half-circle s-curve to distance for sharper (more pixelated) interpolation
    single line formula for Graph Toy:
    0.5 - sqrt(0.25 - (x - step(0.5, x)) * (x - step(0.5, x))) * sign(0.5 - x)
*/

    float x_step = step(0.5, x);
    float curve = 0.5 - sqrt(0.25 - (x - x_step) * (x - x_step)) * sign(0.5 - x);

    return mix(x, curve, sharp);
}

mat4 get_color_matrix(vec2 co, vec2 dx)
{
    return mat4(TEX2D(co - dx), TEX2D(co), TEX2D(co + dx), TEX2D(co + 2.0 * dx));
}

vec3 filter_lanczos(vec4 coeffs, mat4 color_matrix)
{
    vec4 col        = color_matrix * coeffs;
    vec4 sample_min = min(color_matrix[1], color_matrix[2]);
    vec4 sample_max = max(color_matrix[1], color_matrix[2]);

    col = clamp(col, sample_min, sample_max);

    return col.rgb;
}

void main()
{
	vec2 outsize = vec2(textureSize(Sampler0, 0));

    vec2 invSourceSize = 1.0 / SourceSize;
    vec2 dx     = vec2(invSourceSize.x, 0.0);
    vec2 dy     = vec2(0.0, invSourceSize.y);
    vec2 pix_co = vUv * SourceSize - vec2(0.5, 0.5);
    vec2 tex_co = (floor(pix_co) + vec2(0.5, 0.5)) * invSourceSize;
    vec2 dist   = fract(pix_co);
    float curve_x;
    vec3 col, col2;

#if ENABLE_LANCZOS
    curve_x = curve_distance(dist.x, SHARPNESS_H * SHARPNESS_H);

    vec4 coeffs = PI * vec4(1.0 + curve_x, curve_x, 1.0 - curve_x, 2.0 - curve_x);

    coeffs = FIX(coeffs);
    coeffs = 2.0 * sin(coeffs) * sin(coeffs * 0.5) / (coeffs * coeffs);
    coeffs /= dot(coeffs, vec4(1.0));

    col  = filter_lanczos(coeffs, get_color_matrix(tex_co, dx));
    col2 = filter_lanczos(coeffs, get_color_matrix(tex_co + dy, dx));
#else
    curve_x = curve_distance(dist.x, SHARPNESS_H);

    col  = mix(TEX2D(tex_co).rgb,      TEX2D(tex_co + dx).rgb,      curve_x);
    col2 = mix(TEX2D(tex_co + dy).rgb, TEX2D(tex_co + dx + dy).rgb, curve_x);
#endif

    col = mix(col, col2, curve_distance(dist.y, SHARPNESS_V));
    col = pow(col, vec3(GAMMA_INPUT / (DILATION + 1.0)));

    float luma        = dot(vec3(0.2126, 0.7152, 0.0722), col);
    float bright      = (max(col.r, max(col.g, col.b)) + luma) * 0.5;
    float scan_bright = clamp(bright, SCANLINE_BRIGHT_MIN, SCANLINE_BRIGHT_MAX);
    float scan_beam   = clamp(bright * SCANLINE_BEAM_WIDTH_MAX, SCANLINE_BEAM_WIDTH_MIN, SCANLINE_BEAM_WIDTH_MAX);
    float scan_weight = 1.0 - pow(cos(vUv.y * 2.0 * PI * SourceSize.y) * 0.5 + 0.5, scan_beam) * SCANLINE_STRENGTH;

    float mask   = 1.0 - MASK_STRENGTH;    
    vec2 mod_fac = floor(vUv * outsize / vec2(MASK_SIZE, MASK_DOT_HEIGHT * MASK_SIZE));
    int dot_no   = int(mod((mod_fac.x + mod(mod_fac.y, 2.0) * MASK_STAGGER) / MASK_DOT_WIDTH, 3.0));
    vec3 mask_weight;

    if      (dot_no == 0) mask_weight = vec3(1.0,  mask, mask);
    else if (dot_no == 1) mask_weight = vec3(mask, 1.0,  mask);
    else                  mask_weight = vec3(mask, mask, 1.0);

    if (InputSize.y >= SCANLINE_CUTOFF) 
        scan_weight = 1.0;

    col2 = col.rgb;
    col *= vec3(scan_weight);
    col  = mix(col, col2, scan_bright);
    col *= mask_weight;
    col  = pow(col, vec3(1.0 / GAMMA_OUTPUT));

    pColor = vec4(col * BRIGHT_BOOST, 1.0);
})GLSL";


}  // namespace rh::shaders
