/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "OpenGLRenderer.h"

#include <limits>
#include <stdexcept>
#include <string>

#ifndef DISABLE_OPENGL
    #include "../drawing/engines/opengl/OpenGLAPI.h"
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
                for (size_t i = 0; i < kCapabilities.size(); ++i)
                    glGetIntegerv(kCapabilities[i], &_enabled[i]);
            }

            ~SavedState()
            {
                glUseProgram(_program);
                glBindVertexArray(_vertexArray);
                glBindBuffer(GL_ARRAY_BUFFER, _buffer);
                glDepthFunc(_depthFunction);
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
uniform vec4 uView[4];
uniform vec2 uScale;
flat out uint vColour;
void main()
{
    vec3 p = (uView[0] * aPosition.x + uView[1] * aPosition.y
            + uView[2] * aPosition.z + uView[3]).xyz;
    // Positive camera Z, near plane 4, infinite far plane.
    gl_Position = vec4(p.xy * uScale, p.z - 8.0, p.z);
    vColour = uint(aColour);
}
)glsl";

        constexpr const char* kFragmentShader = R"glsl(#version 330 core
flat in uint vColour;
layout(location = 0) out uint oColour;
void main()
{
    oColour = vColour;
}
)glsl";
    } // namespace

    struct OpenGLRenderer::Resources
    {
        GLuint program{}, vertexArray{}, buffer{};
        GLint view{}, scale{};

        ~Resources()
        {
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
            glGenVertexArrays(1, &vertexArray);
            glGenBuffers(1, &buffer);
            if (vertexArray == 0 || buffer == 0 || view < 0 || scale < 0)
                throw std::runtime_error("Unable to allocate the 3D vertex buffer or shader uniforms.");
        }
    };
#else
    struct OpenGLRenderer::Resources
    {
    };
#endif

    OpenGLRenderer::OpenGLRenderer() = default;
    OpenGLRenderer::~OpenGLRenderer() = default;

    void OpenGLRenderer::beginFrame(int32_t width, int32_t height, const Camera& camera)
    {
        _width = width;
        _height = height;
        _camera = camera;
        _vertices.clear();
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

    void OpenGLRenderer::render()
    {
#ifndef DISABLE_OPENGL
        if (_width <= 0 || _height <= 0)
            return;
        if (_vertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
            throw std::runtime_error("3D geometry exceeds the OpenGL draw limit.");

        const SavedState state;
        if (!_resources)
        {
            auto resources = std::make_unique<Resources>();
            resources->initialise();
            _resources = std::move(resources);
        }
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
        glBindVertexArray(_resources->vertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, _resources->buffer);
        glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(Vertex), _vertices.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, colour)));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(_vertices.size()));

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
