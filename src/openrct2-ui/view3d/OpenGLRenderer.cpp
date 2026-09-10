/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "OpenGLRenderer.h"

#include "TileTexture.h"

#include <limits>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/FilterPaletteIds.h>
#include <stdexcept>
#include <string>

#ifndef DISABLE_OPENGL
    #include "../drawing/engines/opengl/OpenGLAPI.h"
    #include "../drawing/engines/opengl/OpenGLFramebuffer.h"
#endif

namespace OpenRCT2::Ui::View3D
{
#ifndef DISABLE_OPENGL
    namespace
    {
        // Keep the host's cached program and actual GL state in agreement, including on failure.
        class SavedState
        {
            GLint _program{}, _vertexArray{}, _buffer{}, _depthFunction{};
            GLint _activeTexture{}, _texture{}, _unpackBuffer{};
            GLint _drawFramebuffer{}, _readFramebuffer{};
            std::array<GLint, 3> _textures2D{};
            std::array<GLint, 4> _viewport{};
            static constexpr std::array<GLenum, 6> kUnpackSettings = {
                GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_IMAGE_HEIGHT,
                GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_IMAGES
            };
            std::array<GLint, kUnpackSettings.size()> _unpack{};
            static constexpr std::array<GLenum, 4> kCapabilities = {
                GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE, GL_SCISSOR_TEST
            };
            std::array<GLint, kCapabilities.size()> _enabled{};

        public:
            SavedState()
            {
                glGetIntegerv(GL_CURRENT_PROGRAM, &_program);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &_vertexArray);
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_buffer);
                glGetIntegerv(GL_DEPTH_FUNC, &_depthFunction);
                glGetIntegerv(GL_ACTIVE_TEXTURE, &_activeTexture);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &_drawFramebuffer);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &_readFramebuffer);
                glGetIntegerv(GL_VIEWPORT, _viewport.data());
                for (size_t i = 0; i < _textures2D.size(); ++i)
                {
                    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
                    glGetIntegerv(GL_TEXTURE_BINDING_2D, &_textures2D[i]);
                }
                glActiveTexture(GL_TEXTURE0);
                glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &_texture);
                glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &_unpackBuffer);
                for (size_t i = 0; i < kUnpackSettings.size(); ++i)
                    glGetIntegerv(kUnpackSettings[i], &_unpack[i]);
                for (size_t i = 0; i < kCapabilities.size(); ++i)
                    glGetIntegerv(kCapabilities[i], &_enabled[i]);
            }

            ~SavedState()
            {
                glUseProgram(_program);
                glBindVertexArray(_vertexArray);
                glBindBuffer(GL_ARRAY_BUFFER, _buffer);
                glDepthFunc(_depthFunction);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _drawFramebuffer);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, _readFramebuffer);
                glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
                for (size_t i = 0; i < _textures2D.size(); ++i)
                {
                    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
                    glBindTexture(GL_TEXTURE_2D, _textures2D[i]);
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D_ARRAY, _texture);
                glActiveTexture(_activeTexture);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _unpackBuffer);
                for (size_t i = 0; i < kUnpackSettings.size(); ++i)
                    glPixelStorei(kUnpackSettings[i], _unpack[i]);
                for (size_t i = 0; i < kCapabilities.size(); ++i)
                {
                    if (_enabled[i])
                        glEnable(kCapabilities[i]);
                    else
                        glDisable(kCapabilities[i]);
                }
            }
        };

        class Shader
        {
        public:
            GLuint id;

            Shader(GLenum type, const char* source)
                : id(glCreateShader(type))
            {
                glShaderSource(id, 1, &source, nullptr);
                glCompileShader(id);
                GLint compiled{};
                glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);
                if (!compiled)
                {
                    char log[1024]{};
                    glGetShaderInfoLog(id, sizeof(log), nullptr, log);
                    glDeleteShader(id);
                    throw std::runtime_error(std::string("3D shader compilation failed: ") + log);
                }
            }

            ~Shader()
            {
                glDeleteShader(id);
            }
        };

        constexpr const char* kVertexShader = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in float aColour;
layout(location = 2) in vec3 aTexture;
layout(location = 3) in float aCutout;
uniform vec4 uView[4];
uniform vec2 uScale;
flat out uint vColour;
out vec2 vUV;
flat out int vLayer;
flat out int vCutout;
void main()
{
    vec3 p = (uView[0] * aPosition.x + uView[1] * aPosition.y
            + uView[2] * aPosition.z + uView[3]).xyz;
    // Positive camera Z, near plane 4, infinite far plane.
    gl_Position = vec4(p.xy * uScale, p.z - 8.0, p.z);
    vColour = uint(aColour);
    vUV = aTexture.xy;
    vLayer = int(aTexture.z);
    vCutout = int(aCutout);
}
)glsl";

        constexpr const char* kFragmentShader = R"glsl(#version 330 core
flat in uint vColour;
in vec2 vUV;
flat in int vLayer;
flat in int vCutout;
uniform usampler2DArray uTiles;
layout(location = 0) out uint oColour;
void main()
{
    oColour = vColour;
    if (vLayer >= 0)
    {
        ivec2 size = textureSize(uTiles, 0).xy;
        ivec2 pixel = clamp(ivec2(floor(vUV * vec2(size))), ivec2(0), size - 1);
        uint colour = texelFetch(uTiles, ivec3(pixel, vLayer), 0).r;
        if (colour != 0u)
            oColour = colour;
        else if (vCutout != 0)
            discard;
    }
}
)glsl";

        constexpr const char* kWaterFragmentShader = R"glsl(#version 330 core
in vec2 vUV;
flat in int vLayer;
uniform usampler2DArray uTiles;
uniform usampler2D uScene;
uniform usampler2D uWaterPalette;
uniform int uWaterOverlay;
layout(location = 0) out uint oColour;
void main()
{
    ivec2 size = textureSize(uTiles, 0).xy;
    ivec2 pixel = clamp(ivec2(floor(vUV * vec2(size))), ivec2(0), size - 1);
    uint mask = vLayer < 0 ? 1u : texelFetch(uTiles, ivec3(pixel, vLayer), 0).r;
    if (mask == 0u)
        discard;

    uint terrain = texelFetch(uScene, ivec2(gl_FragCoord.xy), 0).r;
    int row = min(int(mask), textureSize(uWaterPalette, 0).y - 1);
    oColour = texelFetch(uWaterPalette, ivec2(int(terrain), row), 0).r;
    if (uWaterOverlay >= 0)
    {
        uint ripple = texelFetch(uTiles, ivec3(pixel, uWaterOverlay), 0).r;
        if (ripple != 0u)
            oColour = ripple;
    }
}
)glsl";
    } // namespace

    struct OpenGLRenderer::Resources
    {
        GLuint program{}, vertexArray{}, buffer{}, texture{};
        GLuint waterProgram{}, waterPaletteTexture{};
        GLint view{}, scale{}, tiles{}, textureCapacity{}, maxTextureLayers{};
        GLint waterView{}, waterScale{}, waterTiles{}, waterScene{}, waterPalette{}, waterOverlay{};
        std::unique_ptr<OpenGLFramebuffer> opaqueScene;
        std::vector<Drawing::PaletteIndex> waterPalettePixels;

        ~Resources()
        {
            if (waterPaletteTexture != 0)
                glDeleteTextures(1, &waterPaletteTexture);
            if (waterProgram != 0)
                glDeleteProgram(waterProgram);
            if (texture != 0)
                glDeleteTextures(1, &texture);
            if (buffer != 0)
                glDeleteBuffers(1, &buffer);
            if (vertexArray != 0)
                glDeleteVertexArrays(1, &vertexArray);
            if (program != 0)
                glDeleteProgram(program);
        }

        void initialise()
        {
            const Shader vertex(GL_VERTEX_SHADER, kVertexShader);
            const Shader fragment(GL_FRAGMENT_SHADER, kFragmentShader);
            program = glCreateProgram();
            glAttachShader(program, vertex.id);
            glAttachShader(program, fragment.id);
            glLinkProgram(program);
            GLint linked{};
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (!linked)
            {
                char log[1024]{};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                throw std::runtime_error(std::string("3D shader link failed: ") + log);
            }
            view = glGetUniformLocation(program, "uView[0]");
            scale = glGetUniformLocation(program, "uScale");
            tiles = glGetUniformLocation(program, "uTiles");
            glGenVertexArrays(1, &vertexArray);
            glGenBuffers(1, &buffer);
            glGenTextures(1, &texture);
            glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxTextureLayers);
            if (vertexArray == 0 || buffer == 0 || texture == 0 || view < 0 || scale < 0 || tiles < 0 || maxTextureLayers < 1)
                throw std::runtime_error("Unable to allocate the 3D vertex buffer or shader uniforms.");
        }

        void uploadTextures(TileTextureCache& cache)
        {
            auto entries = cache.entries();
            if (entries.size() > static_cast<size_t>(maxTextureLayers))
                throw std::runtime_error("The park uses more tile textures than this OpenGL device supports.");

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
            if (textureCapacity == 0 || entries.size() > static_cast<size_t>(textureCapacity))
            {
                textureCapacity = std::min(
                    maxTextureLayers, std::max(static_cast<GLint>(entries.size()), std::max(16, textureCapacity * 2)));
                glTexImage3D(
                    GL_TEXTURE_2D_ARRAY, 0, GL_R8UI, kTileTextureSize, kTileTextureSize, textureCapacity, 0, GL_RED_INTEGER,
                    GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                for (auto& entry : entries)
                    entry.dirty = true;
            }
            for (size_t i = 0; i < entries.size(); ++i)
            {
                if (!entries[i].dirty)
                    continue;
                glTexSubImage3D(
                    GL_TEXTURE_2D_ARRAY, 0, 0, 0, static_cast<GLint>(i), kTileTextureSize, kTileTextureSize, 1,
                    GL_RED_INTEGER, GL_UNSIGNED_BYTE, entries[i].pixels.data());
                entries[i].dirty = false;
            }
        }

        void prepareWater(int32_t width, int32_t height)
        {
            if (waterProgram == 0)
            {
                const Shader vertex(GL_VERTEX_SHADER, kVertexShader);
                const Shader fragment(GL_FRAGMENT_SHADER, kWaterFragmentShader);
                waterProgram = glCreateProgram();
                glAttachShader(waterProgram, vertex.id);
                glAttachShader(waterProgram, fragment.id);
                glLinkProgram(waterProgram);
                GLint linked{};
                glGetProgramiv(waterProgram, GL_LINK_STATUS, &linked);
                if (!linked)
                {
                    char log[1024]{};
                    glGetProgramInfoLog(waterProgram, sizeof(log), nullptr, log);
                    throw std::runtime_error(std::string("3D water shader link failed: ") + log);
                }
                waterView = glGetUniformLocation(waterProgram, "uView[0]");
                waterScale = glGetUniformLocation(waterProgram, "uScale");
                waterTiles = glGetUniformLocation(waterProgram, "uTiles");
                waterScene = glGetUniformLocation(waterProgram, "uScene");
                waterPalette = glGetUniformLocation(waterProgram, "uWaterPalette");
                waterOverlay = glGetUniformLocation(waterProgram, "uWaterOverlay");
                glGenTextures(1, &waterPaletteTexture);
                if (waterPaletteTexture == 0 || waterView < 0 || waterScale < 0 || waterTiles < 0 || waterScene < 0
                    || waterPalette < 0 || waterOverlay < 0)
                    throw std::runtime_error("Unable to allocate the 3D water material.");
            }

            GLint drawFramebuffer{}, readFramebuffer{};
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
            glActiveTexture(GL_TEXTURE1);
            if (!opaqueScene || opaqueScene->GetWidth() != static_cast<uint32_t>(width)
                || opaqueScene->GetHeight() != static_cast<uint32_t>(height))
                opaqueScene = std::make_unique<OpenGLFramebuffer>(width, height, false);

            // Copy only the opaque colour buffer. Keep the host depth buffer for shoreline/occlusion testing.
            opaqueScene->BindDraw();
            if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Unable to allocate the underwater scene framebuffer.");
            glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
            glBindTexture(GL_TEXTURE_2D, opaqueScene->GetTexture());

            const auto paletteImage = GetPaletteG1Index(Drawing::FilterPaletteID::paletteWater);
            auto pixels = BuildWaterBlendPalette(paletteImage ? GfxGetG1Element(*paletteImage) : nullptr);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, waterPaletteTexture);
            if (pixels != waterPalettePixels)
            {
                glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_R8UI, 256, static_cast<GLsizei>(pixels.size() / 256), 0, GL_RED_INTEGER,
                    GL_UNSIGNED_BYTE, pixels.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                waterPalettePixels = std::move(pixels);
            }
        }
    };
#else
    struct OpenGLRenderer::Resources
    {
    };
#endif

    OpenGLRenderer::OpenGLRenderer()
        : _textures(std::make_unique<TileTextureCache>())
    {
    }
    OpenGLRenderer::~OpenGLRenderer() = default;

    void OpenGLRenderer::beginFrame(int32_t width, int32_t height, const Camera& camera)
    {
        _width = width;
        _height = height;
        _camera = camera;
        _vertices.clear();
        _waterVertices.clear();
        _waterOverlay = -1;
    }

    void OpenGLRenderer::triangle(Vector a, Vector b, Vector c, uint8_t colour)
    {
        const auto normal = (b - a).cross(c - a);
        const auto length = std::sqrt(normal.dot(normal));
        if (length < 0.001f)
            return;
        const auto light = std::abs(normal.dot({ -0.3f, -0.4f, 0.8660254f })) / length;
        colour += static_cast<uint8_t>(3 + light * 6);
        const auto shade = static_cast<float>(colour);
        _vertices.push_back({ a, shade });
        _vertices.push_back({ b, shade });
        _vertices.push_back({ c, shade });
    }

    void OpenGLRenderer::texturedTriangle(
        Vector a, Vector b, Vector c, Vector origin, const ImageId& image, uint8_t colour, bool cutout)
    {
        const auto layer = _textures->getLayer(image);
        if (layer < 0)
        {
            triangle(a, b, c, colour);
            return;
        }
        for (const auto position : { a, b, c })
        {
            auto uv = TileTextureCoordinates(position, origin);
            uv.z = static_cast<float>(layer);
            _vertices.push_back({ position, static_cast<float>(colour), uv, cutout ? 1.0f : 0.0f });
        }
    }

    void OpenGLRenderer::waterQuad(
        Vector a, Vector b, Vector c, Vector d, Vector origin, const ImageId& mask, const ImageId& overlay)
    {
        const auto layer = _textures->getLayer(mask);
        _waterOverlay = _textures->getLayer(overlay);
        for (const auto position : { a, b, c, a, c, d })
        {
            auto uv = TileTextureCoordinates(position, origin);
            uv.z = static_cast<float>(layer);
            _waterVertices.push_back({ position, 130, uv, 0 });
        }
    }

    void OpenGLRenderer::render()
    {
#ifndef DISABLE_OPENGL
        if (_width <= 0 || _height <= 0)
            return;
        if (_vertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max())
            || _waterVertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
            throw std::runtime_error("3D geometry exceeds the OpenGL draw limit.");

        const SavedState state;
        if (!_resources)
        {
            auto resources = std::make_unique<Resources>();
            resources->initialise();
            _resources = std::move(resources);
        }
        _resources->uploadTextures(*_textures);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        constexpr GLuint background[4] = { 138, 0, 0, 0 };
        constexpr GLfloat depth = 1.0f;
        glClearBufferuiv(GL_COLOR, 0, background);
        glClearBufferfv(GL_DEPTH, 0, &depth);

        glUseProgram(_resources->program);
        const auto origin = _camera.toCamera({});
        const auto x = _camera.toCamera(_camera.position() + Vector{ 1, 0, 0 });
        const auto y = _camera.toCamera(_camera.position() + Vector{ 0, 1, 0 });
        const auto z = _camera.toCamera(_camera.position() + Vector{ 0, 0, 1 });
        const GLfloat view[16] = { x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0,
                                  origin.x, origin.y, origin.z, 1 };
        glUniform4fv(_resources->view, 4, view);
        glUniform2f(_resources->scale, 1.7320508f * _height / _width, 1.7320508f);
        glUniform1i(_resources->tiles, 0);
        glBindVertexArray(_resources->vertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, _resources->buffer);
        glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(Vertex), _vertices.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, colour)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texture)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, cutout)));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(_vertices.size()));

        if (!_waterVertices.empty())
        {
            _resources->prepareWater(_width, _height);
            glUseProgram(_resources->waterProgram);
            glUniform4fv(_resources->waterView, 4, view);
            glUniform2f(_resources->waterScale, 1.7320508f * _height / _width, 1.7320508f);
            glUniform1i(_resources->waterTiles, 0);
            glUniform1i(_resources->waterScene, 1);
            glUniform1i(_resources->waterPalette, 2);
            glUniform1i(_resources->waterOverlay, _waterOverlay);
            glBufferData(GL_ARRAY_BUFFER, _waterVertices.size() * sizeof(Vertex), _waterVertices.data(), GL_STREAM_DRAW);
            // Each fragment blends with the opaque snapshot, never with another tile's already-tinted water.
            // Depth writes select the nearest water surface independently of map traversal order.
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(_waterVertices.size()));
        }

        // The host draws the controls label next, with its own 2D depth convention.
        glClearBufferfv(GL_DEPTH, 0, &depth);
        const auto error = glGetError();
        if (error != GL_NO_ERROR)
            throw std::runtime_error("OpenGL 3D drawing failed (error " + std::to_string(error) + ").");
#else
        throw std::runtime_error("This build does not include OpenGL support.");
#endif
    }
} // namespace OpenRCT2::Ui::View3D
