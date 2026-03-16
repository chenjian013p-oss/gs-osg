// Embedded GLSL for GaussianNode - included once in GaussianNode.cpp

namespace osgGaussian {
namespace Shaders {

const char* frustumCullCS = R"(
#version 430
layout(local_size_x = 256) in;

struct Gaussian {
    vec3 position;
    float pad0;
    vec3 color;
    float opacity;
    vec3 scale;
    float pad1;
    vec4 quat;
    float lodLevel;
};

layout(std430, binding = 0) buffer GaussianBuffer {
    Gaussian gaussians[];
};

layout(std430, binding = 1) buffer VisibleIndexBuffer {
    uint visibleIndices[];
};

layout(std430, binding = 2) buffer CounterBuffer {
    uint visibleCount;
};

uniform mat4 uViewProj;
uniform vec4 uPlane[6];
uniform float uMinScreenSize;
uniform float uMaxScreenSize;
uniform vec2 uViewportSize;
uniform uint uTotalCount;

bool inFrustum(vec3 p) {
    for (int i = 0; i < 6; ++i) {
        if (dot(uPlane[i].xyz, p) + uPlane[i].w < 0.0) return false;
    }
    return true;
}

float screenSize(vec3 pos, float r) {
    vec4 c = uViewProj * vec4(pos, 1.0);
    if (c.w <= 0.0) return 0.0;
    float ndc = r / c.w;
    return ndc * 0.5 * (uViewportSize.x + uViewportSize.y);
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= uTotalCount) return;

    Gaussian g = gaussians[id];
    if (!inFrustum(g.position)) return;

    float r = max(g.scale.x, max(g.scale.y, g.scale.z));
    float sz = screenSize(g.position, r);
    if (sz < uMinScreenSize || sz > uMaxScreenSize) return;

    uint idx = atomicAdd(visibleCount, 1u);
    visibleIndices[idx] = id;
}
)";

const char* depthSortCS = R"(
#version 430
layout(local_size_x = 256) in;

struct Gaussian {
    vec3 position;
    float pad0;
    vec3 color;
    float opacity;
    vec3 scale;
    float pad1;
    vec4 quat;
    float lodLevel;
};

layout(std430, binding = 0) buffer GaussianBuffer { Gaussian gaussians[]; };
layout(std430, binding = 1) buffer VisibleIndexBuffer { uint visibleIndices[]; };
layout(std430, binding = 3) buffer SortedIndexBuffer { uint sortedIndices[]; };
layout(std430, binding = 2) buffer CounterBuffer { uint visibleCount; };

uniform mat4 uView;
uniform uint uVisibleCount;

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= uVisibleCount) return;

    uint gid = visibleIndices[id];
    vec4 v = uView * vec4(gaussians[gid].position, 1.0);
    float depth = -v.z;

    sortedIndices[id] = gid;
}
)";

// Vertex shader: 调试版常量小点（完全不依赖 scale/uSplatScale），用于消除“大红球”
const char* gaussianVert = R"(
#version 330
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4 osg_ModelViewProjectionMatrix;

out vec4 vColor;

void main() {
    gl_Position = osg_ModelViewProjectionMatrix * vec4(aPos, 1.0);
    vColor = aColor;
    gl_PointSize = 3.0; // 固定 3 像素的小点
}
)";

// Vertex shader: SSBO path — no vertex attributes, read by gl_VertexID (layout matches GaussianPoint)
const char* gaussianVertSSBO = R"(
#version 430
// SSBO path: no vertex attributes; fetch by gl_VertexID (layout matches GaussianPoint)

struct GaussianData {
    vec3 position; float pad0;
    vec3 color;    float opacity;
    vec3 scale;    float pad1;
    vec4 quat;     float lodLevel;
};
layout(std430, binding = 0) buffer Gaussians { GaussianData gaussians[]; };
layout(std430, binding = 1) buffer VisibleIndices { uint indices[]; };

uniform mat4 osg_ModelViewProjectionMatrix;
uniform float uPointSizeScale;
uniform float uSplatScale;
uniform vec2 uViewportSize;

out vec4 vColor;
out vec3 vConic;

void main() {
    uint idx = indices[gl_VertexID];
    GaussianData g = gaussians[idx];
    vec3 aPos = g.position;
    vec4 aColor = vec4(g.color, g.opacity);
    vec3 aScale = g.scale;

    vec4 clip = osg_ModelViewProjectionMatrix * vec4(aPos, 1.0);
    gl_Position = clip;
    vColor = aColor;
    float w = clip.w;
    if (w <= 0.0) { gl_PointSize = 1.0; vConic = vec3(1.0, 0.0, 1.0); return; }

    float splatScale = max(uSplatScale, 0.001);
    vec3 scale = aScale * splatScale;
    float r = max(scale.x, max(scale.y, scale.z));

    float sigmaNdc = max(r / w, 1e-4);
    float invSigma2 = 1.0 / (sigmaNdc * sigmaNdc);
    vConic = vec3(invSigma2, 0.0, invSigma2);

    float viewportMax = max(max(uViewportSize.x, uViewportSize.y), 100.0);
    float radiusPix = 3.0 * sigmaNdc * 0.5 * viewportMax;
    float px = radiusPix * 2.0 * uPointSizeScale;
    gl_PointSize = max(1.0, min(512.0, px));
}
)";

// Fragment shader: 实心小圆点，直接用颜色
const char* gaussianFrag = R"(
#version 330
in vec4 vColor;
out vec4 fragColor;

void main() {
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    if (dot(d, d) > 1.0) discard; // 圆形
    fragColor = vColor;
}
)";

} // namespace Shaders
} // namespace osgGaussian
