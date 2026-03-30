#include <OpenGLBackend/GLDevice.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cassert>
#include <cstring>

namespace RiftCore {

    static void GLAPIENTRY GLDebugCallback(
        GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam
    ) {
        RIFTCORE_UNUSED(source);
        RIFTCORE_UNUSED(id);
        RIFTCORE_UNUSED(length);
        RIFTCORE_UNUSED(userParam);
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
        const char* typeStr =
            (type == GL_DEBUG_TYPE_ERROR)   ? "ERROR"   :
            (type == 0x824C)                ? "WARNING" : "INFO";
        std::cerr << "[OpenGL " << typeStr << "] " << message << "\n";
    }

    static u32 GetGLTarget(BufferUsage usage) {
        switch(usage) {
            case BufferUsage::Vertex:  return GL_ARRAY_BUFFER;
            case BufferUsage::Index:   return GL_ELEMENT_ARRAY_BUFFER;
            case BufferUsage::Uniform: return GL_UNIFORM_BUFFER;
            default:                   return GL_ARRAY_BUFFER;
        }
    }

    // ── GLBuffer ──────────────────────────────────────────────
    GLBuffer::GLBuffer(const BufferDesc& desc)
        : size_(desc.size)
        , usage_(desc.usage)
        , glTarget_(GetGLTarget(desc.usage))
    {
        glGenBuffers(1, &glID_);
        glBindBuffer(glTarget_, glID_);
        GLenum glUsage = desc.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
        glBufferData(glTarget_,
                     static_cast<GLsizeiptr>(desc.size),
                     desc.initData, glUsage);
        glBindBuffer(glTarget_, 0);
    }

    GLBuffer::~GLBuffer() {
        if (glID_) { glDeleteBuffers(1, &glID_); glID_ = 0; }
    }

    void* GLBuffer::Map() {
        glBindBuffer(glTarget_, glID_);
        return glMapBuffer(glTarget_, GL_READ_WRITE);
    }

    void GLBuffer::Unmap() {
        glUnmapBuffer(glTarget_);
        glBindBuffer(glTarget_, 0);
    }

    void GLBuffer::Upload(const void* data, usize size) {
        glBindBuffer(glTarget_, glID_);
        glBufferSubData(glTarget_, 0,
                        static_cast<GLsizeiptr>(size), data);
        glBindBuffer(glTarget_, 0);
    }

    usize       GLBuffer::GetSize()  const { return size_;  }
    BufferUsage GLBuffer::GetUsage() const { return usage_; }

    // ── GLTexture ─────────────────────────────────────────────
    GLTexture::GLTexture(const TextureDesc& desc)
        : width_(desc.width), height_(desc.height)
        , mipLevels_(desc.mipLevels), format_(desc.format)
    {
        glGenTextures(1, &glID_);
        glBindTexture(GL_TEXTURE_2D, glID_);

        GLenum internalFmt = GL_RGBA8;
        GLenum fmt         = GL_RGBA;
        GLenum type        = GL_UNSIGNED_BYTE;

        if (desc.format == TextureFormat::RGBA16F) {
            internalFmt = GL_RGBA16F;
            fmt  = GL_RGBA;
            type = GL_HALF_FLOAT;
        } else if (desc.format == TextureFormat::Depth32F) {
            internalFmt = GL_DEPTH_COMPONENT32F;
            fmt  = GL_DEPTH_COMPONENT;
            type = GL_FLOAT;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt,
                     desc.width, desc.height, 0,
                     fmt, type, desc.initData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLTexture::~GLTexture() {
        if (glID_) { glDeleteTextures(1, &glID_); glID_ = 0; }
    }

    // ── GLPipeline ────────────────────────────────────────────
    GLPipeline::GLPipeline(const PipelineDesc& desc)
        : debugName_(desc.debugName ? desc.debugName : "Pipeline")
    {
        if (!desc.shaders.vertexSource ||
            !desc.shaders.fragmentSource) return;

        u32 vert = CompileShader(GL_VERTEX_SHADER,
                                  desc.shaders.vertexSource);
        u32 frag = CompileShader(GL_FRAGMENT_SHADER,
                                  desc.shaders.fragmentSource);

        if (vert && frag) LinkProgram(vert, frag);
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);

        if (desc.depthTest) glEnable(GL_DEPTH_TEST);
        if (desc.blending) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    GLPipeline::~GLPipeline() {
        if (programID_) { glDeleteProgram(programID_); programID_ = 0; }
    }

    u32 GLPipeline::CompileShader(u32 type, const char* source) {
        u32 shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            std::cerr << "[GLPipeline] Shader error: " << log << "\n";
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    bool GLPipeline::LinkProgram(u32 vert, u32 frag) {
        programID_ = glCreateProgram();
        glAttachShader(programID_, vert);
        glAttachShader(programID_, frag);
        glLinkProgram(programID_);
        GLint ok = 0;
        glGetProgramiv(programID_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(programID_, 512, nullptr, log);
            std::cerr << "[GLPipeline] Link error: " << log << "\n";
            glDeleteProgram(programID_);
            programID_ = 0;
            return false;
        }
        return true;
    }

    void GLPipeline::Bind() {
        if (programID_) glUseProgram(programID_);
    }

    const char* GLPipeline::GetDebugName() const {
        return debugName_.c_str();
    }

    void GLPipeline::SetUniformMat4(const char* name, const f32* data) {
        GLint loc = glGetUniformLocation(programID_, name);
        if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, data);
    }

    void GLPipeline::SetUniformVec3(
        const char* name, f32 x, f32 y, f32 z) {
        GLint loc = glGetUniformLocation(programID_, name);
        if (loc >= 0) glUniform3f(loc, x, y, z);
    }

    void GLPipeline::SetUniformInt(const char* name, i32 value) {
        GLint loc = glGetUniformLocation(programID_, name);
        if (loc >= 0) glUniform1i(loc, value);
    }

    void GLPipeline::SetUniformFloat(const char* name, f32 value) {
        GLint loc = glGetUniformLocation(programID_, name);
        if (loc >= 0) glUniform1f(loc, value);
    }

    // ── GLCommandList ─────────────────────────────────────────
    GLCommandList::GLCommandList() {
        glGenVertexArrays(1, &vao_);
    }

    GLCommandList::~GLCommandList() {
        if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    }

    void GLCommandList::Begin() {
        currentPipeline_ = nullptr;
        currentVBO_      = nullptr;
        currentIBO_      = nullptr;
    }

    void GLCommandList::End() {}

    void GLCommandList::SetPipeline(IRHIPipeline* pipeline) {
        currentPipeline_ = reinterpret_cast<GLPipeline*>(pipeline);
        if (currentPipeline_) currentPipeline_->Bind();
    }

    void GLCommandList::SetVertexBuffer(IRHIBuffer* buffer) {
        currentVBO_ = reinterpret_cast<GLBuffer*>(buffer);
    }

    void GLCommandList::SetIndexBuffer(IRHIBuffer* buffer) {
        currentIBO_ = reinterpret_cast<GLBuffer*>(buffer);
    }

    void GLCommandList::SetViewport(const Viewport& vp) {
        glViewport(
            static_cast<GLint>(vp.x),
            static_cast<GLint>(vp.y),
            static_cast<GLsizei>(vp.width),
            static_cast<GLsizei>(vp.height)
        );
    }

    void GLCommandList::Draw(u32 vertexCount, u32 startVertex) {
        if (!currentVBO_ || !currentPipeline_) return;

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, currentVBO_->GetGLID());

        // Layout: vec3 position + vec3 color = 6 floats per vertex
        GLsizei stride = 6 * sizeof(f32);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            stride, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            stride, reinterpret_cast<void*>(3 * sizeof(f32)));

        glDrawArrays(GL_TRIANGLES,
                     static_cast<GLint>(startVertex),
                     static_cast<GLsizei>(vertexCount));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void GLCommandList::DrawIndexed(
        u32 indexCount, u32 startIndex, i32 baseVertex
    ) {
        if (!currentVBO_ || !currentIBO_) return;
        RIFTCORE_UNUSED(startIndex);
        RIFTCORE_UNUSED(baseVertex);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER,
                     currentVBO_->GetGLID());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                     currentIBO_->GetGLID());

        GLsizei stride = 6 * sizeof(f32);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            stride, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            stride, reinterpret_cast<void*>(3 * sizeof(f32)));

        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indexCount),
                       GL_UNSIGNED_INT, nullptr);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void GLCommandList::ClearColor(f32 r, f32 g, f32 b, f32 a) {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void GLCommandList::ClearDepth(f32 depth) {
        glClearDepth(static_cast<GLdouble>(depth));
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // ── GLDevice ──────────────────────────────────────────────
    GLDevice::GLDevice()  = default;
    GLDevice::~GLDevice() { Shutdown(); }

    VoidResult GLDevice::Initialize(void* windowHandle) {
        RIFTCORE_UNUSED(windowHandle);

        if (!glfwInit()) {
            return VoidResult::Err("GLFW init failed");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE,
                       GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

        window_ = glfwCreateWindow(
            1280, 720,
            "RiftCore Engine",
            nullptr, nullptr
        );

        if (!window_) {
            glfwTerminate();
            return VoidResult::Err("GLFW window creation failed");
        }

        glfwMakeContextCurrent(window_);

        if (!gladLoadGLLoader(
                (GLADloadproc)glfwGetProcAddress)) {
            glfwDestroyWindow(window_);
            glfwTerminate();
            return VoidResult::Err("GLAD init failed");
        }

        glfwSwapInterval(1);

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

        gpuName_ = reinterpret_cast<const char*>(
            glGetString(GL_RENDERER));

        std::cout << "[GLDevice] GPU:    " << gpuName_      << "\n";
        std::cout << "[GLDevice] OpenGL: "
                  << glGetString(GL_VERSION)                 << "\n";
        std::cout << "[GLDevice] GLSL:   "
                  << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

        return VoidResult::Ok();
    }

    void GLDevice::Shutdown() {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
            glfwTerminate();
        }
        std::cout << "[GLDevice] Shutdown.\n";
    }

    Result<IRHIBuffer*> GLDevice::CreateBuffer(
        const BufferDesc& desc
    ) {
        return Result<IRHIBuffer*>::Ok(
            static_cast<IRHIBuffer*>(new GLBuffer(desc))
        );
    }

    Result<IRHITexture*> GLDevice::CreateTexture(
        const TextureDesc& desc
    ) {
        return Result<IRHITexture*>::Ok(
            static_cast<IRHITexture*>(new GLTexture(desc))
        );
    }

    Result<IRHIPipeline*> GLDevice::CreatePipeline(
        const PipelineDesc& desc
    ) {
        auto* p = new GLPipeline(desc);
        if (!p->IsValid()) {
            delete p;
            return Result<IRHIPipeline*>::Err(
                "Shader compilation failed");
        }
        return Result<IRHIPipeline*>::Ok(
            static_cast<IRHIPipeline*>(p)
        );
    }

    Result<IRHICommandList*> GLDevice::CreateCommandList() {
        return Result<IRHICommandList*>::Ok(
            static_cast<IRHICommandList*>(new GLCommandList())
        );
    }

    void GLDevice::DestroyBuffer(IRHIBuffer* b) {
        delete reinterpret_cast<GLBuffer*>(b);
    }
    void GLDevice::DestroyTexture(IRHITexture* t) {
        delete reinterpret_cast<GLTexture*>(t);
    }
    void GLDevice::DestroyPipeline(IRHIPipeline* p) {
        delete reinterpret_cast<GLPipeline*>(p);
    }
    void GLDevice::DestroyCommandList(IRHICommandList* c) {
        delete reinterpret_cast<GLCommandList*>(c);
    }

    void GLDevice::BeginFrame() {
        frameIndex_++;
        glfwPollEvents();
    }

    void GLDevice::EndFrame()  {}
    void GLDevice::WaitIdle()  { glFinish(); }

    void GLDevice::Present() {
        if (window_) glfwSwapBuffers(window_);
    }

    bool GLDevice::ShouldClose() const {
        return window_ ? glfwWindowShouldClose(window_) : true;
    }

    void GLDevice::PollEvents() {
        glfwPollEvents();
    }

    // ── OpenGLRHI ─────────────────────────────────────────────
    OpenGLRHI::OpenGLRHI()  = default;
    OpenGLRHI::~OpenGLRHI() = default;

    Result<IRHIDevice*> OpenGLRHI::CreateDevice(
        void* windowHandle
    ) {
        device_ = std::make_unique<GLDevice>();
        auto r  = device_->Initialize(windowHandle);
        if (r.IsErr()) {
            device_.reset();
            return Result<IRHIDevice*>::Err(r.Error());
        }
        return Result<IRHIDevice*>::Ok(
            static_cast<IRHIDevice*>(device_.get())
        );
    }

    void OpenGLRHI::DestroyDevice(IRHIDevice* device) {
        RIFTCORE_UNUSED(device);
        device_.reset();
    }

} // namespace RiftCore

