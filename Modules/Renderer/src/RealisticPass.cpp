// Realistic render mode: physically based shading, world-space (triplanar)
// texturing, a sun shadow map, a procedural sky with image-based ambient,
// transparent glass and filmic tone mapping. The Solid mode in
// RenderSystem.cpp stays as the fast modelling view.

#include "RealisticPass.h"

#include <OpenGLBackend/GLDevice.h>
#include <Renderer/TextureLoader.h>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <tuple>

namespace RiftCore {

    namespace {

        const char* kMeshVert = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aColor;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uLightVP;
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColor;
out vec4 vLightPos;
void main() {
    vec4 wp     = uModel * vec4(aPosition, 1.0);
    vWorldPos   = wp.xyz;
    // Inverse-transpose so normals stay correct on non-uniformly scaled boxes.
    vNormal     = transpose(inverse(mat3(uModel))) * aNormal;
    vUV         = aTexCoord;
    vColor      = aColor;
    vLightPos   = uLightVP * wp;
    gl_Position = uViewProj * wp;
}
)";

        const char* kMeshFrag = R"(
#version 460 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vColor;
in vec4 vLightPos;
out vec4 fragColor;

uniform vec3  uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uOpacity;
uniform float uTexScale;      // metres per texture repeat; 0 = untextured
uniform int   uHasAlbedo;
uniform int   uHasNormal;
uniform int   uHasRough;
uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uRoughTex;
uniform sampler2DShadow uShadowMap;
uniform int   uShadows;
uniform vec3  uCameraPos;
uniform vec3  uSunDir;        // direction the light travels
uniform vec3  uSunColor;
uniform vec3  uSkyColor;
uniform vec3  uHorizonColor;
uniform vec3  uGroundColor;
uniform float uExposure;
uniform int   uStyle;         // 0 realistic, 1 shaded, 2 hidden line

const float PI = 3.14159265;

vec3 toLinear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

vec3 skyRadiance(vec3 d) {
    float t = clamp(d.y, -1.0, 1.0);
    vec3 up = mix(uHorizonColor, uSkyColor, pow(max(t, 0.0), 0.5));
    return t >= 0.0 ? up : mix(uHorizonColor, uGroundColor, clamp(-t * 4.0, 0.0, 1.0));
}

float shadowFactor(vec3 N, vec3 L) {
    if (uShadows == 0) return 1.0;
    vec3 p = vLightPos.xyz / vLightPos.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = max(0.0022 * (1.0 - dot(N, L)), 0.0006);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float sum = 0.0;
    for (int x = -2; x <= 2; x++)
        for (int y = -2; y <= 2; y++)
            sum += texture(uShadowMap, vec3(p.xy + vec2(x, y) * texel * 1.2, p.z - bias));
    return sum / 25.0;
}

float distGGX(float NdH, float a) {
    float a2 = a * a;
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float geomSmith(float NdV, float NdL, float r) {
    float k = (r + 1.0) * (r + 1.0) / 8.0;
    return (NdV / (NdV * (1.0 - k) + k)) * (NdL / (NdL * (1.0 - k) + k));
}
vec3 fresnel(float c, vec3 F0) { return F0 + (1.0 - F0) * pow(1.0 - c, 5.0); }
vec3 fresnelRough(float c, vec3 F0, float r) {
    return F0 + (max(vec3(1.0 - r), F0) - F0) * pow(1.0 - c, 5.0);
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    if (dot(N, V) < 0.0 && uOpacity < 1.0) N = -N;

    vec3  base  = toLinear(uAlbedo) * toLinear(vColor);
    float rough = uRoughness;

    if (uTexScale > 0.0) {
        // Triplanar projection in world metres: textures keep their true
        // size on every face, however the box under them was scaled.
        vec3 w = pow(abs(N), vec3(6.0));
        w /= (w.x + w.y + w.z);
        vec3 p = vWorldPos / uTexScale;
        vec2 ux = p.zy, uy = p.xz, uz = p.xy;
        if (uHasAlbedo != 0) {
            vec3 t = texture(uAlbedoTex, ux).rgb * w.x + texture(uAlbedoTex, uy).rgb * w.y
                   + texture(uAlbedoTex, uz).rgb * w.z;
            base = toLinear(t) * toLinear(uAlbedo);
        }
        if (uHasRough != 0) {
            rough = texture(uRoughTex, ux).g * w.x + texture(uRoughTex, uy).g * w.y
                  + texture(uRoughTex, uz).g * w.z;
        }
        if (uHasNormal != 0) {
            vec3 nx = texture(uNormalTex, ux).xyz * 2.0 - 1.0;
            vec3 ny = texture(uNormalTex, uy).xyz * 2.0 - 1.0;
            vec3 nz = texture(uNormalTex, uz).xyz * 2.0 - 1.0;
            nx = vec3(nx.xy + N.zy, abs(nx.z) * N.x);
            ny = vec3(ny.xy + N.xz, abs(ny.z) * N.y);
            nz = vec3(nz.xy + N.xy, abs(nz.z) * N.z);
            N = normalize(nx.zyx * w.x + ny.xzy * w.y + nz.xyz * w.z);
        }
    }
    rough = clamp(rough, 0.05, 1.0);

    if (uStyle != 0) {
        // CAD styles: even, shadowless light from the sun side so every face reads.
        float l = max(dot(N, normalize(-uSunDir)), 0.0);
        float side = 0.5 + 0.5 * N.y;
        vec3 c = uStyle == 2 ? vec3(0.90 + 0.10 * l)
                             : pow(base, vec3(1.0 / 2.2)) * (0.62 + 0.30 * l + 0.08 * side);
        fragColor = vec4(c, 1.0);
        return;
    }

    vec3  L   = normalize(-uSunDir);
    vec3  H   = normalize(L + V);
    float NdL = max(dot(N, L), 0.0);
    float NdV = max(dot(N, V), 0.001);
    float NdH = max(dot(N, H), 0.0);
    vec3  F0  = mix(vec3(0.04), base, uMetallic);

    // Direct sun.
    vec3  F    = fresnel(max(dot(H, V), 0.0), F0);
    float D    = distGGX(NdH, rough * rough);
    float G    = geomSmith(NdV, NdL, rough);
    vec3  spec = D * G * F / max(4.0 * NdV * NdL, 0.001);
    vec3  kd   = (1.0 - F) * (1.0 - uMetallic);
    float sh   = shadowFactor(N, L);
    vec3  direct = (kd * base / PI + spec) * uSunColor * NdL * sh;

    // Ambient from the sky: diffuse irradiance plus a blurred reflection.
    vec3 Fa      = fresnelRough(NdV, F0, rough);
    vec3 irr     = mix(skyRadiance(N), (uSkyColor + uHorizonColor + uGroundColor) / 3.0, 0.65);
    vec3 R       = reflect(-V, N);
    vec3 refl    = mix(skyRadiance(R), irr, rough * rough);
    vec3 ambient = (1.0 - Fa) * (1.0 - uMetallic) * base * irr + Fa * refl;
    // Ambient occlusion we cannot compute: darken where the sun is blocked.
    ambient *= mix(0.72, 1.0, sh);

    vec3 color = (direct + ambient) * uExposure;
    float alpha = uOpacity;
    if (alpha < 1.0) alpha = clamp(alpha + (Fa.r + Fa.g + Fa.b) / 3.0, 0.0, 1.0);

    // Light haze with distance.
    float dist = length(uCameraPos - vWorldPos);
    color = mix(color, uHorizonColor * uExposure, 1.0 - exp(-dist * 0.0022));

    fragColor = vec4(pow(aces(color), vec3(1.0 / 2.2)), alpha);
}
)";

        const char* kDepthVert = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uLightVP;
void main() { gl_Position = uLightVP * uModel * vec4(aPosition, 1.0); }
)";
        const char* kDepthFrag = "#version 460 core\nvoid main() {}\n";

        const char* kLineFrag = R"(
#version 460 core
out vec4 fragColor;
uniform vec3 uColor;
void main() { fragColor = vec4(uColor, 1.0); }
)";

        const char* kSkyVert = R"(
#version 460 core
out vec2 vNdc;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    vNdc = p;
    gl_Position = vec4(p, 1.0, 1.0);
}
)";
        const char* kSkyFrag = R"(
#version 460 core
in vec2 vNdc;
out vec4 fragColor;
uniform vec3 uForward, uRight, uUp;
uniform float uTanX, uTanY;
uniform vec3 uSunDir, uSunColor, uSkyColor, uHorizonColor, uGroundColor;
uniform float uExposure;
vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec3 d = normalize(uForward + uRight * vNdc.x * uTanX + uUp * vNdc.y * uTanY);
    float t = d.y;
    vec3 c = t >= 0.0 ? mix(uHorizonColor, uSkyColor, pow(t, 0.5))
                      : mix(uHorizonColor, uGroundColor, clamp(-t * 4.0, 0.0, 1.0));
    float s = max(dot(d, normalize(-uSunDir)), 0.0);
    c += uSunColor * (pow(s, 900.0) * 6.0 + pow(s, 24.0) * 0.06);
    fragColor = vec4(pow(aces(c * uExposure), vec3(1.0 / 2.2)), 1.0);
}
)";

        GLuint Compile(GLenum type, const char* src) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[2048];
                glGetShaderInfoLog(s, sizeof(log), nullptr, log);
                std::cerr << "[Realistic] shader compile error:\n" << log << "\n";
            }
            return s;
        }

        GLuint Link(const char* vs, const char* fs) {
            GLuint v = Compile(GL_VERTEX_SHADER, vs), f = Compile(GL_FRAGMENT_SHADER, fs);
            GLuint p = glCreateProgram();
            glAttachShader(p, v);
            glAttachShader(p, f);
            glLinkProgram(p);
            GLint ok = 0;
            glGetProgramiv(p, GL_LINK_STATUS, &ok);
            if (!ok) {
                char log[2048];
                glGetProgramInfoLog(p, sizeof(log), nullptr, log);
                std::cerr << "[Realistic] program link error:\n" << log << "\n";
                glDeleteProgram(p);
                p = 0;
            }
            glDeleteShader(v);
            glDeleteShader(f);
            return p;
        }

        void U3(GLuint p, const char* n, const Vec3& v) { glUniform3f(glGetUniformLocation(p, n), v.x, v.y, v.z); }
        void U1(GLuint p, const char* n, f32 v)        { glUniform1f(glGetUniformLocation(p, n), v); }
        void UI(GLuint p, const char* n, int v)        { glUniform1i(glGetUniformLocation(p, n), v); }
        void UM(GLuint p, const char* n, const Mat4& m){ glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, m.DataPtr()); }

        Mat4 Ortho(f32 r, f32 nearZ, f32 farZ) {
            Mat4 m = Mat4::Identity();
            m.cols[0][0] = 1.0f / r;
            m.cols[1][1] = 1.0f / r;
            m.cols[2][2] = -2.0f / (farZ - nearZ);
            m.cols[3][2] = -(farZ + nearZ) / (farZ - nearZ);
            return m;
        }

    } // namespace

    RealisticPass::RealisticPass() = default;

    RealisticPass::~RealisticPass() {
        for (auto& [mesh, vao] : vaos_) glDeleteVertexArrays(1, &vao);
        if (meshProgram_)  glDeleteProgram(meshProgram_);
        if (depthProgram_) glDeleteProgram(depthProgram_);
        if (skyProgram_)   glDeleteProgram(skyProgram_);
        if (lineProgram_)  glDeleteProgram(lineProgram_);
        for (auto& [mesh, e] : edges_) { glDeleteVertexArrays(1, &e.vao); glDeleteBuffers(1, &e.ibo); }
        if (shadowFbo_)    glDeleteFramebuffers(1, &shadowFbo_);
        if (shadowTex_)    glDeleteTextures(1, &shadowTex_);
        if (skyVao_)       glDeleteVertexArrays(1, &skyVao_);
    }

    bool RealisticPass::Init() {
        if (ready_) return true;
        meshProgram_  = Link(kMeshVert, kMeshFrag);
        depthProgram_ = Link(kDepthVert, kDepthFrag);
        skyProgram_   = Link(kSkyVert, kSkyFrag);
        lineProgram_  = Link(kDepthVert, kLineFrag);
        if (!meshProgram_ || !depthProgram_ || !skyProgram_ || !lineProgram_) return false;

        glGenVertexArrays(1, &skyVao_);
        glGenTextures(1, &shadowTex_);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowSize, kShadowSize, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const f32 border[4] = { 1, 1, 1, 1 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        GLint prev = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
        glGenFramebuffers(1, &shadowFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev));
        ready_ = ok;
        return ok;
    }

    void RealisticPass::ForgetMesh(const GPUMesh* mesh) {
        auto e = edges_.find(mesh);
        if (e != edges_.end()) {
            glDeleteVertexArrays(1, &e->second.vao);
            glDeleteBuffers(1, &e->second.ibo);
            edges_.erase(e);
        }
        auto it = vaos_.find(mesh);
        if (it == vaos_.end()) return;
        glDeleteVertexArrays(1, &it->second);
        vaos_.erase(it);
    }

    // Feature edges: mesh edges on the outline (one triangle) or on a crease
    // (two triangles whose faces meet at more than about 20 degrees). The
    // diagonals inside flat quads are therefore not drawn, as in a CAD view.
    const RealisticPass::EdgeGL& RealisticPass::EdgesFor(const GPUMesh* mesh) {
        auto it = edges_.find(mesh);
        if (it != edges_.end()) return it->second;
        EdgeGL out;

        GLuint vbo = static_cast<GLBuffer*>(mesh->vertexBuffer)->GetGLID();
        GLuint ibo = static_cast<GLBuffer*>(mesh->indexBuffer)->GetGLID();
        std::vector<f32> v(static_cast<size_t>(mesh->vertexCount) * 11);
        std::vector<u32> idx(mesh->indexCount);
        if (!v.empty() && !idx.empty()) {
            glGetNamedBufferSubData(vbo, 0, static_cast<GLsizeiptr>(v.size() * sizeof(f32)), v.data());
            glGetNamedBufferSubData(ibo, 0, static_cast<GLsizeiptr>(idx.size() * sizeof(u32)), idx.data());

            // Weld vertices by position so faces that share an edge are found.
            std::map<std::tuple<int, int, int>, u32> weld;
            std::vector<u32> wid(mesh->vertexCount);
            for (u32 i = 0; i < mesh->vertexCount; i++) {
                auto key = std::make_tuple(static_cast<int>(std::lround(v[i * 11] * 2000.0f)),
                                           static_cast<int>(std::lround(v[i * 11 + 1] * 2000.0f)),
                                           static_cast<int>(std::lround(v[i * 11 + 2] * 2000.0f)));
                wid[i] = weld.emplace(key, static_cast<u32>(weld.size())).first->second;
            }
            struct Edge { u32 a, b; Vec3 n; int faces; bool crease; };
            std::map<std::pair<u32, u32>, Edge> table;
            for (size_t t = 0; t + 2 < idx.size(); t += 3) {
                u32 i[3] = { idx[t], idx[t + 1], idx[t + 2] };
                if (i[0] >= mesh->vertexCount || i[1] >= mesh->vertexCount || i[2] >= mesh->vertexCount) continue;
                Vec3 p[3];
                for (int k = 0; k < 3; k++) p[k] = { v[i[k] * 11], v[i[k] * 11 + 1], v[i[k] * 11 + 2] };
                Vec3 e1 = { p[1].x - p[0].x, p[1].y - p[0].y, p[1].z - p[0].z };
                Vec3 e2 = { p[2].x - p[0].x, p[2].y - p[0].y, p[2].z - p[0].z };
                Vec3 n  = Math::Cross(e1, e2);
                if (n.x * n.x + n.y * n.y + n.z * n.z < 1e-14f) continue;      // degenerate
                n = Math::Normalize(n);
                for (int k = 0; k < 3; k++) {
                    u32 a = i[k], b = i[(k + 1) % 3];
                    u32 wa = wid[a], wb = wid[b];
                    if (wa == wb) continue;
                    auto key = wa < wb ? std::make_pair(wa, wb) : std::make_pair(wb, wa);
                    auto f = table.find(key);
                    if (f == table.end()) {
                        table[key] = { a, b, n, 1, false };
                    } else {
                        if (Math::Dot(f->second.n, n) < 0.94f) f->second.crease = true;
                        f->second.faces++;
                    }
                }
            }
            std::vector<u32> lines;
            for (auto& [key, e] : table) {
                if (e.faces == 1 || e.crease) { lines.push_back(e.a); lines.push_back(e.b); }
            }
            if (!lines.empty()) {
                glGenVertexArrays(1, &out.vao);
                glBindVertexArray(out.vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glGenBuffers(1, &out.ibo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ibo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(lines.size() * sizeof(u32)),
                             lines.data(), GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(f32), nullptr);
                glBindVertexArray(0);
                out.count = static_cast<int>(lines.size());
            }
        }
        edges_[mesh] = out;
        return edges_[mesh];
    }

    unsigned int RealisticPass::VaoFor(const GPUMesh* mesh) {
        auto it = vaos_.find(mesh);
        if (it != vaos_.end()) return it->second;
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLBuffer*>(mesh->vertexBuffer)->GetGLID());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLBuffer*>(mesh->indexBuffer)->GetGLID());
        const GLsizei stride = 11 * sizeof(f32);
        const int sizes[4] = { 3, 3, 2, 3 };
        size_t offset = 0;
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offset));
            offset += sizes[i] * sizeof(f32);
        }
        glBindVertexArray(0);
        vaos_[mesh] = vao;
        return vao;
    }

    void RealisticPass::Render(const std::vector<DrawCall>& draws, const std::vector<Light>& lights,
                               const Camera& camera, u32 width, u32 height, RenderStats& stats)
    {
        if (!Init()) return;

        Light sun;
        sun.direction = { -0.4f, -1.0f, -0.5f };
        sun.color     = { 1.0f, 0.96f, 0.88f };
        sun.intensity = 1.8f;
        for (auto& l : lights) if (l.type == LightType::Directional) { sun = l; break; }
        Vec3 dir = Math::Normalize(sun.direction);
        if (dir.y > -0.08f) { dir.y = -0.08f; dir = Math::Normalize(dir); }
        Vec3 sunColor = { sun.color.x * sun.intensity * 2.2f, sun.color.y * sun.intensity * 2.2f,
                          sun.color.z * sun.intensity * 2.2f };

        // ---- shadow frustum: fit the buildings, not the endless ground ----
        Vec3 lo = { 1e9f, 1e9f, 1e9f }, hi = { -1e9f, -1e9f, -1e9f };
        for (auto& dc : draws) {
            const f32* m = dc.transform.DataPtr();
            f32 sx = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
            f32 sy = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
            f32 sz = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
            f32 r  = 0.5f * std::max({ sx, sy, sz });
            if (r > 22.0f) continue;
            lo = { std::min(lo.x, m[12] - r), std::min(lo.y, m[13] - r), std::min(lo.z, m[14] - r) };
            hi = { std::max(hi.x, m[12] + r), std::max(hi.y, m[13] + r), std::max(hi.z, m[14] + r) };
        }
        if (lo.x > hi.x) { lo = { -10, 0, -10 }; hi = { 10, 10, 10 }; }
        Vec3 centre = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
        f32 radius = 0.5f * std::sqrt((hi.x-lo.x)*(hi.x-lo.x) + (hi.y-lo.y)*(hi.y-lo.y) + (hi.z-lo.z)*(hi.z-lo.z));
        radius = std::clamp(radius, 6.0f, 70.0f);
        Vec3 eye = { centre.x - dir.x * radius * 2.0f, centre.y - dir.y * radius * 2.0f,
                     centre.z - dir.z * radius * 2.0f };
        Vec3 up = std::fabs(dir.y) > 0.98f ? Vec3{ 0, 0, 1 } : Vec3{ 0, 1, 0 };
        Mat4 lightVP = Math::Multiply(Ortho(radius, 0.1f, radius * 4.0f), Math::LookAt(eye, centre, up));

        GLint targetFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &targetFbo);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // ---- pass 1: shadow depth ----
        const bool cad = style_ != 0;
        const bool useShadows = shadows_ && !cad;
        if (useShadows) {
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
            glViewport(0, 0, kShadowSize, kShadowSize);
            glClear(GL_DEPTH_BUFFER_BIT);
            glUseProgram(depthProgram_);
            UM(depthProgram_, "uLightVP", lightVP);
            glDisable(GL_CULL_FACE);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);
            for (auto& dc : draws) {
                if (!dc.mesh || !dc.mesh->indexBuffer || dc.material.opacity < 0.99f) continue;
                UM(depthProgram_, "uModel", dc.transform);
                glBindVertexArray(VaoFor(dc.mesh));
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(dc.mesh->indexCount), GL_UNSIGNED_INT, nullptr);
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        // ---- pass 2: sky, opaque, then glass ----
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFbo));
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        if (style_ == 2)      glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        else if (cad)         glClearColor(0.88f, 0.90f, 0.93f, 1.0f);
        else                  glClearColor(0.72f, 0.82f, 0.94f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const Vec3 sky = { 0.20f, 0.42f, 0.86f }, horizon = { 0.72f, 0.82f, 0.94f }, ground = { 0.26f, 0.25f, 0.23f };
        const Mat4& proj = camera.GetProjectionMatrix();

        const bool drawSky = !cad && !camera.IsOrthographic();
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glUseProgram(skyProgram_);
        U3(skyProgram_, "uForward", camera.GetForward());
        U3(skyProgram_, "uRight", camera.GetRight());
        U3(skyProgram_, "uUp", camera.GetUp());
        U1(skyProgram_, "uTanX", 1.0f / proj.cols[0][0]);
        U1(skyProgram_, "uTanY", 1.0f / proj.cols[1][1]);
        U3(skyProgram_, "uSunDir", dir);
        U3(skyProgram_, "uSunColor", sunColor);
        U3(skyProgram_, "uSkyColor", sky);
        U3(skyProgram_, "uHorizonColor", horizon);
        U3(skyProgram_, "uGroundColor", ground);
        U1(skyProgram_, "uExposure", exposure_);
        glBindVertexArray(skyVao_);
        if (drawSky) glDrawArrays(GL_TRIANGLES, 0, 3);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);

        GLuint p = meshProgram_;
        glUseProgram(p);
        UM(p, "uViewProj", camera.GetViewProjection());
        UM(p, "uLightVP", lightVP);
        U3(p, "uCameraPos", camera.GetPosition());
        U3(p, "uSunDir", dir);
        U3(p, "uSunColor", sunColor);
        U3(p, "uSkyColor", sky);
        U3(p, "uHorizonColor", horizon);
        U3(p, "uGroundColor", ground);
        U1(p, "uExposure", exposure_);
        UI(p, "uShadows", useShadows ? 1 : 0);
        UI(p, "uStyle", style_);
        UI(p, "uAlbedoTex", 0);
        UI(p, "uNormalTex", 1);
        UI(p, "uRoughTex", 2);
        UI(p, "uShadowMap", 3);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);

        auto drawOne = [&](const DrawCall& dc) {
            const MaterialData& m = dc.material;
            UM(p, "uModel", dc.transform);
            U3(p, "uAlbedo", m.albedo);
            U1(p, "uMetallic", m.metallic);
            U1(p, "uRoughness", m.roughness);
            U1(p, "uOpacity", cad ? 1.0f : m.opacity);
            U1(p, "uTexScale", cad ? 0.0f : m.texScale);
            const Texture2D* tex[3] = { m.albedoTex, m.normalTex, m.roughTex };
            const char* has[3] = { "uHasAlbedo", "uHasNormal", "uHasRough" };
            for (int i = 0; i < 3; i++) {
                bool on = tex[i] && tex[i]->isValid() && m.texScale > 0.0f;
                UI(p, has[i], on ? 1 : 0);
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, on ? tex[i]->glID : 0);
            }
            glBindVertexArray(VaoFor(dc.mesh));
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(dc.mesh->indexCount), GL_UNSIGNED_INT, nullptr);
            stats.drawCalls++;
            stats.triangles += dc.mesh->indexCount / 3;
        };

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        std::vector<const DrawCall*> glass;
        if (cad) {                                  // push faces back so the edge lines win
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
        }
        for (auto& dc : draws) {
            if (!dc.mesh || !dc.mesh->indexBuffer) continue;
            if (!cad && dc.material.opacity < 0.99f) { glass.push_back(&dc); continue; }
            drawOne(dc);
        }
        if (cad) {
            glDisable(GL_POLYGON_OFFSET_FILL);
            glUseProgram(lineProgram_);
            UM(lineProgram_, "uLightVP", camera.GetViewProjection());
            U3(lineProgram_, "uColor", style_ == 2 ? Vec3{ 0.0f, 0.0f, 0.0f } : Vec3{ 0.10f, 0.11f, 0.13f });
            glLineWidth(1.0f);
            for (auto& dc : draws) {
                if (!dc.mesh || !dc.mesh->indexBuffer) continue;
                const EdgeGL& e = EdgesFor(dc.mesh);
                if (!e.count) continue;
                UM(lineProgram_, "uModel", dc.transform);
                glBindVertexArray(e.vao);
                glDrawElements(GL_LINES, e.count, GL_UNSIGNED_INT, nullptr);
            }
            glUseProgram(p);
        }

        if (!glass.empty()) {
            Vec3 cp = camera.GetPosition();
            auto dist2 = [&](const DrawCall* dc) {
                const f32* m = dc->transform.DataPtr();
                f32 dx = m[12] - cp.x, dy = m[13] - cp.y, dz = m[14] - cp.z;
                return dx * dx + dy * dy + dz * dz;
            };
            std::sort(glass.begin(), glass.end(), [&](const DrawCall* a, const DrawCall* b) { return dist2(a) > dist2(b); });
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            for (const DrawCall* dc : glass) drawOne(*dc);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
        }

        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
    }

} // namespace RiftCore
