#include <Renderer/TextureLoader.h>

#include <glad/glad.h>
#include <stb_image.h>

#include <iostream>
#include <cstring>
#include <cmath>

namespace RiftCore {

    TextureLoader::TextureLoader() {
        // Flip images vertically by default
        // OpenGL expects (0,0) at bottom-left
        // Most images have (0,0) at top-left
        stbi_set_flip_vertically_on_load(true);
        CreateDefaultTextures();
    }

    TextureLoader::~TextureLoader() {
        UnloadAll();
    }

    u32 TextureLoader::CreateGLTexture(
        const u8* pixels,
        i32 width, i32 height, i32 channels
    ) {
        u32 glID = 0;
        glGenTextures(1, &glID);
        glBindTexture(GL_TEXTURE_2D, glID);

        GLenum internalFmt = (channels == 4)
            ? GL_RGBA8 : GL_RGB8;
        GLenum fmt = (channels == 4)
            ? GL_RGBA  : GL_RGB;

        glTexImage2D(GL_TEXTURE_2D, 0,
            internalFmt, width, height, 0,
            fmt, GL_UNSIGNED_BYTE, pixels);

        // Generate mipmaps for better quality
        glGenerateMipmap(GL_TEXTURE_2D);

        // Texture filtering
        glTexParameteri(GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_LINEAR);

        // Texture wrapping
        glTexParameteri(GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(GL_TEXTURE_2D, 0);
        return glID;
    }

    void TextureLoader::CreateDefaultTextures() {
        // White 1x1 texture
        {
            u8 px[4] = {255, 255, 255, 255};
            white_ = std::make_unique<Texture2D>();
            white_->glID   = CreateGLTexture(px, 1, 1, 4);
            white_->width  = 1;
            white_->height = 1;
            white_->path   = "__white__";
        }

        // Black 1x1 texture
        {
            u8 px[4] = {0, 0, 0, 255};
            black_ = std::make_unique<Texture2D>();
            black_->glID   = CreateGLTexture(px, 1, 1, 4);
            black_->width  = 1;
            black_->height = 1;
            black_->path   = "__black__";
        }

        // Normal map default (flat = pointing up = 128,128,255)
        {
            u8 px[4] = {128, 128, 255, 255};
            normalDefault_ = std::make_unique<Texture2D>();
            normalDefault_->glID   = CreateGLTexture(px,1,1,4);
            normalDefault_->width  = 1;
            normalDefault_->height = 1;
            normalDefault_->path   = "__normal_default__";
        }

        // Checkerboard 128x128
        {
            const i32 sz  = 128;
            const i32 chk = 16;
            std::vector<u8> pixels(sz * sz * 4);

            for (i32 y = 0; y < sz; y++) {
                for (i32 x = 0; x < sz; x++) {
                    bool isLight = ((x/chk) + (y/chk)) % 2 == 0;
                    u8 c = isLight ? 200 : 60;
                    i32 idx = (y * sz + x) * 4;
                    pixels[idx+0] = c;
                    pixels[idx+1] = c;
                    pixels[idx+2] = c;
                    pixels[idx+3] = 255;
                }
            }

            checker_ = std::make_unique<Texture2D>();
            checker_->glID = CreateGLTexture(
                pixels.data(), sz, sz, 4);
            checker_->width  = sz;
            checker_->height = sz;
            checker_->path   = "__checker__";
        }

        std::cout << "[TextureLoader] Default textures created\n";
    }

    Result<Texture2D*> TextureLoader::Load(
        const String& filePath
    ) {
        // Check cache first
        auto it = cache_.find(filePath);
        if (it != cache_.end()) {
            return Result<Texture2D*>::Ok(it->second.get());
        }

        // Load with stb_image
        i32 width, height, channels;
        u8* data = stbi_load(
            filePath.c_str(),
            &width, &height, &channels, 4
        );

        if (!data) {
            return Result<Texture2D*>::Err(
                "Failed to load texture: " + filePath +
                " - " + stbi_failure_reason()
            );
        }

        auto tex = std::make_unique<Texture2D>();
        tex->glID   = CreateGLTexture(data, width, height, 4);
        tex->width  = width;
        tex->height = height;
        tex->path   = filePath;

        stbi_image_free(data);

        std::cout << "[TextureLoader] Loaded: " << filePath
                  << " (" << width << "x" << height << ")\n";

        auto* ptr = tex.get();
        cache_[filePath] = std::move(tex);
        return Result<Texture2D*>::Ok(ptr);
    }

    Result<Texture2D*> TextureLoader::CreateFromPixels(
        const String& name,
        const u8*     pixels,
        i32           width,
        i32           height,
        i32           channels
    ) {
        auto it = cache_.find(name);
        if (it != cache_.end()) {
            return Result<Texture2D*>::Ok(it->second.get());
        }

        auto tex = std::make_unique<Texture2D>();
        tex->glID   = CreateGLTexture(
            pixels, width, height, channels);
        tex->width  = width;
        tex->height = height;
        tex->path   = name;

        auto* ptr = tex.get();
        cache_[name] = std::move(tex);
        return Result<Texture2D*>::Ok(ptr);
    }

    Result<Texture2D*> TextureLoader::CreateSolidColor(
        const String& name,
        u8 r, u8 g, u8 b, u8 a
    ) {
        u8 px[4] = {r, g, b, a};
        return CreateFromPixels(name, px, 1, 1, 4);
    }

    Result<Texture2D*> TextureLoader::CreateCheckerboard(
        const String& name,
        i32  size,
        i32  checkSize,
        u8   colorA[4],
        u8   colorB[4]
    ) {
        u8 defA[4] = {220, 220, 220, 255};
        u8 defB[4] = {60,  60,  60,  255};
        u8* ca = colorA ? colorA : defA;
        u8* cb = colorB ? colorB : defB;

        std::vector<u8> pixels(size * size * 4);
        for (i32 y = 0; y < size; y++) {
            for (i32 x = 0; x < size; x++) {
                bool useA = ((x/checkSize) +
                             (y/checkSize)) % 2 == 0;
                u8* col = useA ? ca : cb;
                i32 idx = (y * size + x) * 4;
                pixels[idx+0] = col[0];
                pixels[idx+1] = col[1];
                pixels[idx+2] = col[2];
                pixels[idx+3] = col[3];
            }
        }
        return CreateFromPixels(
            name, pixels.data(), size, size, 4);
    }

    Result<Texture2D*> TextureLoader::CreateGrid(
        const String& name,
        i32  size,
        i32  gridSize,
        u8   lineWidth
    ) {
        std::vector<u8> pixels(size * size * 4);

        // Fill with base color (light gray)
        for (i32 i = 0; i < size * size * 4; i += 4) {
            pixels[i+0] = 180;
            pixels[i+1] = 180;
            pixels[i+2] = 200;
            pixels[i+3] = 255;
        }

        // Draw grid lines (darker)
        for (i32 y = 0; y < size; y++) {
            for (i32 x = 0; x < size; x++) {
                bool onLine =
                    (x % gridSize) < lineWidth ||
                    (y % gridSize) < lineWidth;
                if (onLine) {
                    i32 idx = (y * size + x) * 4;
                    pixels[idx+0] = 80;
                    pixels[idx+1] = 80;
                    pixels[idx+2] = 100;
                    pixels[idx+3] = 255;
                }
            }
        }

        return CreateFromPixels(
            name, pixels.data(), size, size, 4);
    }

    void TextureLoader::Bind(Texture2D* tex, u32 slot) {
        glActiveTexture(GL_TEXTURE0 + slot);
        if (tex && tex->isValid()) {
            glBindTexture(GL_TEXTURE_2D, tex->glID);
        } else {
            glBindTexture(GL_TEXTURE_2D,
                white_ ? white_->glID : 0);
        }
    }

    void TextureLoader::Unbind(u32 slot) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void TextureLoader::Unload(const String& name) {
        auto it = cache_.find(name);
        if (it == cache_.end()) return;

        if (it->second->glID) {
            glDeleteTextures(1, &it->second->glID);
        }
        cache_.erase(it);
    }

    void TextureLoader::UnloadAll() {
        for (auto& [name, tex] : cache_) {
            if (tex && tex->glID) {
                glDeleteTextures(1, &tex->glID);
            }
        }
        cache_.clear();

        auto deleteBuiltin = [](std::unique_ptr<Texture2D>& t) {
            if (t && t->glID) {
                glDeleteTextures(1, &t->glID);
                t.reset();
            }
        };

        deleteBuiltin(white_);
        deleteBuiltin(black_);
        deleteBuiltin(checker_);
        deleteBuiltin(normalDefault_);
    }

} // namespace RiftCore
