/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Controller.h"

#include "Camera.h"
#include "MapAdapter.h"
#include "OpenGLRenderer.h"
#include "Presentation.h"

#include <SDL.h>
#include <openrct2-ui/input/InputManager.h>
#include <openrct2/Input.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Drawing.Screen.h>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/ui/UiContext.h>
#include <stdexcept>

namespace OpenRCT2::Ui::View3D
{
    struct Controller::Session
    {
        Camera camera;
        OpenGLRenderer renderer;
        Presentation presentation;
        uint64_t lastFrame{};
        bool initialised{};
        bool frameReady{};

        void draw(Drawing::RenderTarget& rt)
        {
            frameReady = false;
            if (rt.width <= 0 || rt.height <= 0)
                return;
            if (!initialised)
            {
                const auto start = GetMapCameraStart();
                camera.initialise(start.center, start.extent, rt.width, rt.height);
                lastFrame = SDL_GetPerformanceCounter();
                initialised = true;
            }
            const auto now = SDL_GetPerformanceCounter();
            const auto dt = static_cast<float>(now - lastFrame) / SDL_GetPerformanceFrequency();
            lastFrame = now;
            const auto* keys = SDL_GetKeyboardState(nullptr);
            const CameraInput input{ static_cast<float>(keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]),
                                     static_cast<float>(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]),
                                     keys[SDL_SCANCODE_LSHIFT] != 0 };
            camera.move(dt, input, GetMapExtent());
            renderer.beginFrame(rt.width, rt.height, camera);
            DrawMap(renderer);
            presentation.drawOverlay(rt);
            frameReady = true;
        }
    };

    Controller::Controller(IUiContext& uiContext, InputManager& inputManager, CursorState& cursorState)
        : _uiContext(uiContext)
        , _inputManager(inputManager)
        , _cursorState(cursorState)
    {
    }

    Controller::~Controller() = default;

    bool Controller::active() const
    {
        return _session != nullptr;
    }

    bool Controller::blocksGameInput() const
    {
        return active() || _restoreInput;
    }

    void Controller::enter()
    {
        if (active())
            return;
#ifdef DISABLE_OPENGL
        _uiContext.ShowMessageBox("3D View requires a build with OpenGL support.");
        return;
#else
        if (Config::Get().general.drawingEngine != DrawingEngine::openGL || SDL_GL_GetCurrentContext() == nullptr)
        {
            _uiContext.ShowMessageBox("3D View requires OpenGL. Select OpenGL in Options > Display > Drawing engine.");
            return;
        }
#endif
        SDL_GetMouseState(&_savedMouseX, &_savedMouseY);
        _savedRelativeMouseMode = SDL_GetRelativeMouseMode() == SDL_TRUE;
        _savedCursorVisibility = SDL_ShowCursor(SDL_QUERY);
        if (SDL_SetRelativeMouseMode(SDL_FALSE) != 0)
        {
            _uiContext.ShowMessageBox(std::string("Unable to enable the visible mouse for 3D View: ") + SDL_GetError());
            return;
        }
        try
        {
            _session = std::make_unique<Session>();
        }
        catch (const std::exception& e)
        {
            SDL_SetRelativeMouseMode(_savedRelativeMouseMode ? SDL_TRUE : SDL_FALSE);
            SDL_ShowCursor(_savedCursorVisibility);
            _uiContext.ShowMessageBox(e.what());
            return;
        }
        _inputManager.reset();
        _cursorState = {};
        SDL_ShowCursor(SDL_ENABLE);
    }

    void Controller::leave()
    {
        if (!active())
            return;
        _session.reset();
        SDL_SetRelativeMouseMode(_savedRelativeMouseMode ? SDL_TRUE : SDL_FALSE);
        SDL_ShowCursor(_savedCursorVisibility);
        SDL_WarpMouseInWindow(static_cast<SDL_Window*>(_uiContext.GetWindow()), _savedMouseX, _savedMouseY);
        _cursorState = {};
        _cursorState.position = { static_cast<int32_t>(_savedMouseX / Config::Get().general.windowScale),
                                  static_cast<int32_t>(_savedMouseY / Config::Get().general.windowScale) };
        _inputManager.reset();
        InputSetState(InputState::reset);
        _restoreInput = true;
        Drawing::GfxInvalidateScreen();
    }

    bool Controller::handleEvent(const SDL_Event& event)
    {
        if (active())
        {
            if ((event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST))
            {
                leave();
            }
            else if (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_RMASK) != 0)
            {
                _session->camera.look(event.motion.xrel, event.motion.yrel);
            }
        }
        return blocksGameInput() && event.type != SDL_QUIT && event.type != SDL_WINDOWEVENT;
    }

    void Controller::finishEvents()
    {
        if (!_restoreInput)
            return;
        int numKeys = 0;
        const auto* keys = SDL_GetKeyboardState(&numKeys);
        if (SDL_GetMouseState(nullptr, nullptr) == 0
            && std::none_of(keys, keys + numKeys, [](uint8_t key) { return key != 0; }))
        {
            _restoreInput = false;
        }
    }

    bool Controller::draw(Drawing::RenderTarget& rt)
    {
        if (!active())
            return false;
        try
        {
            _session->draw(rt);
            return true;
        }
        catch (const std::exception& e)
        {
            leave();
            _uiContext.ShowMessageBox(std::string("Unable to render 3D View: ") + e.what());
            return false;
        }
    }

    void Controller::presentOpenGL()
    {
        if (!active() || !_session->frameReady)
            return;
        _session->frameReady = false;
        try
        {
            _session->presentation.present(_session->renderer);
        }
        catch (const std::exception& e)
        {
            leave();
            _uiContext.ShowMessageBox(std::string("Unable to render 3D View: ") + e.what());
        }
    }
} // namespace OpenRCT2::Ui::View3D
