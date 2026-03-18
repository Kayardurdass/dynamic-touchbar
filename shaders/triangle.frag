#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform UniformBufferObject {
  float time;
} ubo;

// CC0: Saturday cubism experiment
//  Trying to recreate some twitch graphics but failed
//  but thought the failure looked interesting enough to share

#define TIME        ubo.time
#define RESOLUTION  vec2(64, 2048)
#define PI          3.141592654
#define TAU         (2.0*PI)
#define ROT(a)      mat2(cos(a), sin(a), -sin(a), cos(a))

#define TOLERANCE       0.0005
#define MAX_RAY_LENGTH  20.0
#define MAX_RAY_MARCHES 80
#define MAX_SHD_MARCHES 20
#define NORM_OFF        0.005

const mat2 rot0 = ROT(0.0);
mat2 g_rot0 = rot0;

// License: WTFPL, author: sam hocevar, found: https://stackoverflow.com/a/17897228/418488
const vec4 hsv2rgb_K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
vec3 hsv2rgb(vec3 c) {
  vec3 p = abs(fract(c.xxx + hsv2rgb_K.xyz) * 6.0 - hsv2rgb_K.www);
  return c.z * mix(hsv2rgb_K.xxx, clamp(p - hsv2rgb_K.xxx, 0.0, 1.0), c.y);
}
// License: WTFPL, author: sam hocevar, found: https://stackoverflow.com/a/17897228/418488
//  Macro version of above to enable compile-time constants
#define HSV2RGB(c)  (c.z * mix(hsv2rgb_K.xxx, clamp(abs(fract(c.xxx + hsv2rgb_K.xyz) * 6.0 - hsv2rgb_K.www) - hsv2rgb_K.xxx, 0.0, 1.0), c.y))

const float hoff = 0.0;

const vec3 skyCol     = HSV2RGB(vec3(hoff+0.57, 0.90, 0.25));
const vec3 skylineCol = HSV2RGB(vec3(hoff+0.02, 0.95, 0.5));
const vec3 sunCol     = HSV2RGB(vec3(hoff+0.07, 0.95, 0.5));
const vec3 diffCol1   = HSV2RGB(vec3(hoff+0.60, 0.90, 1.0));
const vec3 diffCol2   = HSV2RGB(vec3(hoff+0.55, 0.90, 1.0));

const vec3 sunDir1    = normalize(vec3(0., 0.05, -1.0));

const vec3 lightPos1  = vec3(10.0, 10.0, 10.0);
const vec3 lightPos2  = vec3(-10.0, 10.0, -10.0);
  
// License: Unknown, author: nmz (twitter: @stormoid), found: https://www.shadertoy.com/view/NdfyRM
vec3 sRGB(vec3 t) {
  return mix(1.055*pow(t, vec3(1./2.4)) - 0.055, 12.92*t, step(t, vec3(0.0031308)));
}

// License: Unknown, author: Matt Taylor (https://github.com/64), found: https://64.github.io/tonemapping/
vec3 aces_approx(vec3 v) {
  v = max(v, 0.0);
  v *= 0.6f;
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;
  return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

// License: Unknown, author: Unknown, found: don't remember
float tanh_approx(float x) {
  //  Found this somewhere on the interwebs
  //  return tanh(x);
  float x2 = x*x;
  return clamp(x*(27.0 + x2)/(27.0+9.0*x2), -1.0, 1.0);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/articles/distfunctions/
float rayPlane(vec3 ro, vec3 rd, vec4 p) {
  return -(dot(ro,p.xyz)+p.w)/dot(rd,p.xyz);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float box(vec2 p, vec2 b) {
  vec2 d = abs(p)-b;
  return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

// License: MIT, author: Inigo Quilez, found: https://iquilezles.org/www/articles/distfunctions3d/distfunctions3d.htm
float box(vec3 p, vec3 b) {
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float ref(inout vec3 p, vec3 r) {
  float d = dot(p, r);
  p -= r*min(0.0, d)*2.0;
  return d < 0.0 ? 0.0 : 1.0;
}

vec3 render0(vec3 ro, vec3 rd) {
  vec3 col = vec3(0.0);
  float sf = 1.0001-max(dot(sunDir1, rd), 0.0);
  col += skyCol*pow((1.0-abs(rd.y)), 8.0);
  col += clamp(vec3(mix(0.0025, 0.125, tanh_approx(.005/sf))/abs(rd.y))*skylineCol, 0.0, 10.0);
  sf *= sf;
  col += sunCol*0.00005/sf;

  float tp1  = rayPlane(ro, rd, vec4(vec3(0.0, -1.0, 0.0), 6.0));

  if (tp1 > 0.0) {
    vec3 pos  = ro + tp1*rd;
    vec2 pp = pos.xz;
    float db = box(pp, vec2(5.0, 9.0))-3.0;
    
    col += vec3(4.0)*skyCol*rd.y*rd.y*smoothstep(0.25, 0.0, db);
    col += vec3(0.8)*skyCol*exp(-0.5*max(db, 0.0));
  }

  return clamp(col, 0.0, 10.0);
}

float df(vec3 p) {
  p.xz *= g_rot0;
  vec3 p0 = p;
  vec3 p1 = p;
  vec3 p2 = p;

  const float ss = 1.;
  p0.y -= -0.2;
  p0.z = abs(p0.z);
  p0.x = -abs(p0.x);
  p0.x -= -0.4*ss;
  ref(p0, normalize(vec3(1.0, -0.05, -1.0)));  
  p0.x -= 1.3*ss;
  ref(p0, normalize(vec3(1.0, 0.30, 1.0)));  
  p0.x -= 1.4*ss;
  p0.z -= 0.3*ss;
  ref(p0, normalize(vec3(1.0, -1.0, 0.5)));
  p0.x -= 1.25*ss;
  p0.z -= -0.5*ss;
  p0.y -= -0.3*ss;
  float d0 = box(p0, vec3(0.5))-0.0125;

  p1.x -= 0.4;
  p1.y -= 0.75;
  float d1 = box(p1, vec3(1.25))-0.0125;

  p2.y += 2.0;
  float d2 = p2.y;

  float d = d1;
  d = min(d, d0);
  d = min(d, d2);
  return d;
}

vec3 normal(vec3 pos) {
  vec2  eps = vec2(NORM_OFF,0.0);
  vec3 nor;
  nor.x = df(pos+eps.xyy) - df(pos-eps.xyy);
  nor.y = df(pos+eps.yxy) - df(pos-eps.yxy);
  nor.z = df(pos+eps.yyx) - df(pos-eps.yyx);
  return normalize(nor);
}

float rayMarch(vec3 ro, vec3 rd, float initt) {
  float t = initt;
  float tol = TOLERANCE;
  for (int i = 0; i < MAX_RAY_MARCHES; ++i) {
    if (t > MAX_RAY_LENGTH) {
      t = MAX_RAY_LENGTH;    
      break;
    }
    float d = df(ro + rd*t);
    if (d < TOLERANCE) {
      break;
    }
    t += d;
  }
  return t;
}

float shadow(vec3 lp, vec3 ld, float mint, float maxt) {
  const float ds = 1.0-0.4;
  float t = mint;
  float nd = 1E6;
  float h;
  const float soff = 0.05;
  const float smul = 1.5;
  for (int i=0; i < MAX_SHD_MARCHES; ++i) {
    vec3 p = lp + ld*t;
    float d = df(p);
    if (d < TOLERANCE || t >= maxt) {
      float sd = 1.0-exp(-smul*max(t/maxt-soff, 0.0));
      return t >= maxt ? mix(sd, 1.0, smoothstep(0.0, 0.025, nd)) : sd;
    }
    nd = min(nd, d);
    t += ds*d;
  }
  float sd = 1.0-exp(-smul*max(t/maxt-soff, 0.0));
  return sd;
}

vec3 boxCol(vec3 col, vec3 nsp, vec3 rd, vec3 nnor, vec3 nrcol, float nshd1, float nshd2) {
  float nfre  = 1.0+dot(rd, nnor);
  nfre        *= nfre;

  vec3 nld1   = normalize(lightPos1-nsp); 
  vec3 nld2   = normalize(lightPos2-nsp); 

  float ndif1 = max(dot(nld1, nnor), 0.0);
  ndif1       *= ndif1;

  float ndif2 = max(dot(nld2, nnor), 0.0);
  ndif2       *= ndif2;

  vec3 scol = vec3(0.0);
  float rf = smoothstep(1.0, 0.9, nfre);
  scol += diffCol1*ndif1*nshd1;
  scol += diffCol2*ndif2*nshd2;
  scol += 0.1*(skyCol+skylineCol);
  scol += nrcol*0.75*mix(vec3(0.25), vec3(0.5, 0.5, 1.0), nfre);

  col = mix(col, scol, rf*smoothstep(90.0, 20.0, dot(nsp, nsp)));
  
  return col;
}

vec3 render1(vec3 ro, vec3 rd) {
  vec3 skyCol = render0(ro, rd);
  vec3 col = skyCol;

  float nt    = rayMarch(ro, rd, .0); 
  if (nt < MAX_RAY_LENGTH) {
    vec3 nsp    = ro + rd*nt;
    vec3 nnor   = normal(nsp);

    vec3 nref   = reflect(rd, nnor);
    float nrt   = rayMarch(nsp, nref, 0.2);
    vec3 nrcol  = render0(nsp, nref);
    
    if (nrt < MAX_RAY_LENGTH) {
      vec3 nrsp   = nsp + nref*nrt;
      vec3 nrnor  = normal(nrsp);
      vec3 nrref  = reflect(nref, nrnor);
      nrcol = boxCol(nrcol, nrsp, nref, nrnor, render0(nrsp, nrref), 1.0, 1.0);
    }

    float nshd1  = mix(0.0, 1.0, shadow(nsp, normalize(lightPos1 - nsp), 0.1, distance(lightPos1, nsp)));
    float nshd2  = mix(0.0, 1.0, shadow(nsp, normalize(lightPos2 - nsp), 0.1, distance(lightPos2, nsp)));

    col = boxCol(col, nsp, rd, nnor, nrcol, nshd1, nshd2);    
  }

  return col;
}

vec3 effect(vec2 p) {
  g_rot0 = ROT(-0.2*TIME);
  
  const float fov = tan(TAU/6.0);
  const vec3 ro = vec3(0.0, 2.5, 5.0);
  const vec3 la = vec3(0.0, 0.0, 0.0);
  const vec3 up = vec3(0.1, 1.0, 0.0);

  vec3 ww = normalize(la - ro);
  vec3 uu = normalize(cross(up, ww));
  vec3 vv = cross(ww,uu);
  vec3 rd = normalize(-p.x*uu + p.y*vv + fov*ww);

  vec3 col = render1(ro, rd);
  
  return col;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1. + 2. * q;
  p.x *= RESOLUTION.x/RESOLUTION.y;
  vec3 col = effect(p);
  col = aces_approx(col); 
  col = sRGB(col);

  fragColor = vec4(col, 1.0);
}

void main()
{
  // mainImage(fragColor, gl_FragCoord.xy);
  fragColor = vec4(1.0);
}
