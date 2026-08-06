#pragma warning(disable: 4190)
#include <Renderer/RenderSystem.h>
#include <OpenGLBackend/GLDevice.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>









namespace RiftCore {

    // -- Shaders -----------------------------------------------
    static const char* s_vertSrc = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aColor;

uniform mat4  uModel;
uniform mat4  uViewProj;
uniform mat3  uNormalMatrix;
uniform float uTexTileX;
uniform float uTexTileY;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vColor;

void main() {
    vec4 worldPos  = uModel * vec4(aPosition, 1.0);
    vWorldPos      = worldPos.xyz;
    vNormal        = uNormalMatrix * aNormal;
    vTexCoord      = aTexCoord * vec2(uTexTileX, uTexTileY);
    vColor         = aColor;
    gl_Position    = uViewProj * worldPos;
}
)";

    static const char* s_fragSrc = R"(
#version 460 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vColor;

out vec4 fragColor;

// Material
uniform vec3  uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform int   uHasTexture;
uniform sampler2D uAlbedoTex;

// Lighting
uniform vec3  uCameraPos;
uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform vec3  uAmbient;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0),
                     32.0 * (1.0 - uRoughness));

    // Sample texture if available
    vec3 texColor = (uHasTexture != 0)
        ? texture(uAlbedoTex, vTexCoord).rgb
        : vColor;

    vec3 baseColor = texColor * uAlbedo;
    vec3 ambient   = uAmbient * baseColor;
    vec3 diffuse   = diff * uLightColor
                     * uLightIntensity * baseColor;
    vec3 specular  = spec * uLightColor
                     * uLightIntensity
                     * mix(0.04, 1.0, uMetallic);

    vec3 result = ambient + diffuse + specular;
    result = result / (result + vec3(1.0));
    fragColor = vec4(result, 1.0);
}
)";

    static const char* s_wireVertSrc = R"(
#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMatrix;

void main() {
    gl_Position = uViewProj * uModel * vec4(aPosition, 1.0);
}
)";

    static const char* s_wireFragSrc = R"(
#version 460 core
out vec4 fragColor;
void main() {
    fragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
)";

    // -- RenderSystem ------------------------------------------
    RenderSystem::RenderSystem()  = default;
    RenderSystem::~RenderSystem() { Shutdown(); }

    VoidResult RenderSystem::Initialize(
        IRHIDevice* device,
        u32 width, u32 height
    ) {
        device_ = device;
        width_  = width;
        height_ = height;

        // Each DLL needs its own GLAD initialization
        // because GLAD function pointers are per-DLL
        if (!gladLoadGLLoader(
                (GLADloadproc)glfwGetProcAddress)) {
            return VoidResult::Err(
                "Renderer DLL: GLAD reload failed");
        }
        std::cout << "[Renderer] GLAD reloaded OK\n";

        auto r = CreateShaders();
        if (r.IsErr()) return r;

        auto cmdRes = device_->CreateCommandList();
        if (cmdRes.IsErr()) return VoidResult::Err(
            "CommandList creation failed");
        cmdList_ = cmdRes.Value();

        camera_.SetAspectRatio(
            static_cast<f32>(width) /
            static_cast<f32>(height));
        camera_.SetPosition({0.0f, 2.0f, 6.0f});
        camera_.SetClipPlanes(0.1f, 100.0f);
        camera_.SetFOV(60.0f);

        // Create texture loader
        textures_ = std::make_unique<TextureLoader>();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        std::cout << "[Renderer] Initialized "
                  << width << "x" << height << "\n";
        return VoidResult::Ok();
    }

    VoidResult RenderSystem::CreateShaders() {
        // Main lit pipeline
        PipelineDesc desc;
        desc.shaders.vertexSource   = s_vertSrc;
        desc.shaders.fragmentSource = s_fragSrc;
        desc.depthTest  = true;
        desc.depthWrite = true;
        desc.blending   = false;
        desc.debugName  = "LitPipeline";

        auto r = device_->CreatePipeline(desc);
        if (r.IsErr()) {
            return VoidResult::Err(
                "Lit shader failed: " + r.Error().message);
        }
        pipeline_ = r.Value();

        // Wireframe pipeline
        PipelineDesc wDesc;
        wDesc.shaders.vertexSource   = s_wireVertSrc;
        wDesc.shaders.fragmentSource = s_wireFragSrc;
        wDesc.depthTest  = true;
        wDesc.debugName  = "WirePipeline";

        auto wr = device_->CreatePipeline(wDesc);
        if (wr.IsErr()) {
            return VoidResult::Err(
                "Wire shader failed: " + wr.Error().message);
        }
        wirePipeline_ = wr.Value();

        return VoidResult::Ok();
    }

    void RenderSystem::Shutdown() {
        if (!device_) return;

        for (auto& dc : drawQueue_) {
            RIFTCORE_UNUSED(dc);
        }

        if (textures_) { textures_->UnloadAll(); textures_.reset(); }
        if (cmdList_)     { device_->DestroyCommandList(cmdList_);  cmdList_     = nullptr; }
        if (pipeline_)    { device_->DestroyPipeline(pipeline_);    pipeline_    = nullptr; }
        if (wirePipeline_){ device_->DestroyPipeline(wirePipeline_);wirePipeline_= nullptr; }

        device_ = nullptr;
        std::cout << "[Renderer] Shutdown complete.\n";
    }

    void RenderSystem::BeginFrame(const Camera& camera) {
        camera_   = camera;
        viewProj_ = camera_.GetViewProjection();
        drawQueue_.clear();
        lights_.clear();
        frameIndex_++;

        stats_.drawCalls  = 0;
        stats_.triangles  = 0;
        stats_.lights     = 0;
        stats_.frameIndex = frameIndex_;
    }

    void RenderSystem::Submit(const DrawCall& dc) {
        if (!dc.mesh || !dc.mesh->isValid()) return;
        drawQueue_.push_back(dc);
    }

    void RenderSystem::SubmitLight(const Light& light) {
        lights_.push_back(light);
        stats_.lights++;
    }

    void RenderSystem::EndFrame() {
        // Clear screen
        cmdList_->ClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        cmdList_->ClearDepth(1.0f);

        Viewport vp;
        vp.x=0; vp.y=0;
        vp.width  = static_cast<f32>(width_);
        vp.height = static_cast<f32>(height_);
        cmdList_->SetViewport(vp);

        // Set active pipeline
        IRHIPipeline* activePipe =
            wireframe_ ? wirePipeline_ : pipeline_;
        cmdList_->SetPipeline(activePipe);

        auto* glPipe = static_cast<GLPipeline*>(activePipe);

        // Upload camera uniforms once
        glPipe->SetUniformMat4(
            "uViewProj", viewProj_.DataPtr());

        Vec3 camPos = camera_.GetPosition();
        glPipe->SetUniformVec3(
            "uCameraPos", camPos.x, camPos.y, camPos.z);

        // Upload light (use first light or default)
        if (!lights_.empty()) {
            auto& l = lights_[0];
            glPipe->SetUniformVec3("uLightDir",
                l.direction.x, l.direction.y, l.direction.z);
            glPipe->SetUniformVec3("uLightColor",
                l.color.x, l.color.y, l.color.z);
            glPipe->SetUniformFloat("uLightIntensity",
                l.intensity);
        } else {
            // Default sun light
            glPipe->SetUniformVec3(
                "uLightDir", -0.5f, -1.0f, -0.5f);
            glPipe->SetUniformVec3(
                "uLightColor", 1.0f, 1.0f, 1.0f);
            glPipe->SetUniformFloat("uLightIntensity", 1.0f);
        }

        // Ambient light
        glPipe->SetUniformVec3("uAmbient",
            0.08f, 0.08f, 0.12f);

        // Draw all submitted objects
        for (auto& dc : drawQueue_) {
            RenderDrawCall(dc);
        }
    }

    void RenderSystem::RenderDrawCall(const DrawCall& dc) {
        auto* glPipe = static_cast<GLPipeline*>(
            wireframe_ ? wirePipeline_ : pipeline_);

        // Model matrix
        glPipe->SetUniformMat4("uModel",
            dc.transform.DataPtr());

        // Normal matrix = transpose(inverse(model))
        // For uniform scale we can just use model's
        // upper-left 3x3 directly (simplified)
        // Pass as mat3 via first 9 floats
        const f32* m = dc.transform.DataPtr();
        // Upload mat3 normal matrix
        // For now pass identity-like values
        GLint normLoc = glGetUniformLocation(
            glPipe->GetProgramID(), "uNormalMatrix");
        if (normLoc >= 0) {
            // Extract upper 3x3 from column-major mat4
            f32 nm[9] = {
                m[0], m[1], m[2],
                m[4], m[5], m[6],
                m[8], m[9], m[10]
            };
            glUniformMatrix3fv(normLoc, 1,
                GL_FALSE, nm);
        }

        // Material uniforms
        glPipe->SetUniformVec3("uAlbedo",
            dc.material.albedo.x,
            dc.material.albedo.y,
            dc.material.albedo.z);
        glPipe->SetUniformFloat("uMetallic",
            dc.material.metallic);
        glPipe->SetUniformFloat("uRoughness",
            dc.material.roughness);
        glPipe->SetUniformFloat("uTexTileX",
            dc.material.texTileX);
        glPipe->SetUniformFloat("uTexTileY",
            dc.material.texTileY);

        // Bind albedo texture
        bool hasTexture = (dc.material.albedoTex != nullptr)
                        && dc.material.albedoTex->isValid();
        glPipe->SetUniformInt("uHasTexture",
            hasTexture ? 1 : 0);
        glPipe->SetUniformInt("uAlbedoTex", 0);

        if (textures_) {
            textures_->Bind(
                hasTexture ? dc.material.albedoTex
                           : textures_->GetWhiteTexture(),
                0
            );
        }

        // Bind buffers and draw
        cmdList_->SetVertexBuffer(dc.mesh->vertexBuffer);

        if (dc.mesh->indexBuffer) {
            // Set vertex attrib pointers for Vertex3D layout
            // position(3) + normal(3) + texcoord(2) + color(3)
            // = 11 floats per vertex
            GLuint vao = 0;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glBindBuffer(GL_ARRAY_BUFFER,
                static_cast<GLBuffer*>(
                    dc.mesh->vertexBuffer)->GetGLID());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLBuffer*>(
                    dc.mesh->indexBuffer)->GetGLID());

            GLsizei stride = 11 * sizeof(f32);
            // position
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT,
                GL_FALSE, stride,
                reinterpret_cast<void*>(0));
            // normal
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT,
                GL_FALSE, stride,
                reinterpret_cast<void*>(3*sizeof(f32)));
            // texcoord
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT,
                GL_FALSE, stride,
                reinterpret_cast<void*>(6*sizeof(f32)));
            // color
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT,
                GL_FALSE, stride,
                reinterpret_cast<void*>(8*sizeof(f32)));

            glDrawElements(GL_TRIANGLES,
                static_cast<GLsizei>(dc.mesh->indexCount),
                GL_UNSIGNED_INT, nullptr);

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(3);
            glBindVertexArray(0);
            glDeleteVertexArrays(1, &vao);
        }

        stats_.drawCalls++;
        stats_.triangles += dc.mesh->indexCount / 3;
    }

    void RenderSystem::Present() {
        // Swap happens in device->Present()
    }

    Result<GPUMesh*> RenderSystem::UploadMesh(
        const MeshData& data
    ) {
        auto* mesh = new GPUMesh();
        mesh->name        = data.name;
        mesh->vertexCount = static_cast<u32>(
            data.vertices.size());
        mesh->indexCount  = static_cast<u32>(
            data.indices.size());

        // Upload vertices
        BufferDesc vbDesc;
        vbDesc.size     = data.vertices.size()
                          * sizeof(Vertex3D);
        vbDesc.usage    = BufferUsage::Vertex;
        vbDesc.initData = data.vertices.data();

        auto vr = device_->CreateBuffer(vbDesc);
        if (vr.IsErr()) {
            delete mesh;
            return Result<GPUMesh*>::Err("VBO failed");
        }
        mesh->vertexBuffer = vr.Value();

        // Upload indices
        BufferDesc ibDesc;
        ibDesc.size     = data.indices.size()
                          * sizeof(u32);
        ibDesc.usage    = BufferUsage::Index;
        ibDesc.initData = data.indices.data();

        auto ir = device_->CreateBuffer(ibDesc);
        if (ir.IsErr()) {
            device_->DestroyBuffer(mesh->vertexBuffer);
            delete mesh;
            return Result<GPUMesh*>::Err("IBO failed");
        }
        mesh->indexBuffer = ir.Value();

        std::cout << "[Renderer] Mesh '" << data.name
                  << "' uploaded: "
                  << data.vertices.size() << " verts, "
                  << data.indices.size() / 3
                  << " tris\n";

        return Result<GPUMesh*>::Ok(mesh);
    }

    void RenderSystem::DestroyMesh(GPUMesh* mesh) {
        if (!mesh) return;
        if (mesh->vertexBuffer)
            device_->DestroyBuffer(mesh->vertexBuffer);
        if (mesh->indexBuffer)
            device_->DestroyBuffer(mesh->indexBuffer);
        delete mesh;
    }

    void RenderSystem::OnResize(u32 width, u32 height) {
        width_  = width;
        height_ = height;
        glViewport(0, 0, width, height);
        camera_.SetAspectRatio(
            static_cast<f32>(width) /
            static_cast<f32>(height));
        camera_.Update();
    }

    void RenderSystem::SetWireframe(bool enabled) {
        wireframe_ = enabled;
        glPolygonMode(GL_FRONT_AND_BACK,
            enabled ? GL_LINE : GL_FILL);
    }

    // -- RendererModule ----------------------------------------
    RendererModule::RendererModule()  = default;
    RendererModule::~RendererModule() = default;

    VoidResult RendererModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* log = nullptr;
        if (params.context) {
            log = params.context->Logger();
        }

        if (log) log->Info("Renderer", "Initializing...");

        renderer_ = std::make_unique<RenderSystem>();

        if (log) log->Info("Renderer", "Renderer module ready.");

        return VoidResult::Ok();
    }

    void RendererModule::OnUpdate(f32 deltaTime) {
        RIFTCORE_UNUSED(deltaTime);
    }

    void RendererModule::OnRender() {
        // Rendering is driven by main loop
    }

    void RendererModule::Shutdown() {
        std::cout << "[Renderer] Shutting down...\n";
        if (renderer_) renderer_->Shutdown();
        renderer_.reset();
        std::cout << "[Renderer] Shutdown complete.\n";
    }

    ModuleDescriptor RendererModule::GetDescriptor() const {
        ModuleDescriptor desc;
        desc.name        = "Renderer";
        desc.version     = "0.1.0";
        desc.apiVersion  = RIFTCORE_API_VERSION;
        desc.description = "3D Renderer with lighting";
        return desc;
    }

    RIFTCORE_IMPLEMENT_MODULE(RendererModule)

} // namespace RiftCore





