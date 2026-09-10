/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <memory>

union SDL_Event;

namespace OpenRCT2
{
    struct CursorState;
    namespace Drawing
    {
        struct RenderTarget;
    }
    namespace Ui
    {
        struct IUiContext;
        class InputManager;
    } // namespace Ui
} // namespace OpenRCT2

namespace OpenRCT2::Ui::View3D
{
    class Controller
    {
    public:
        Controller(IUiContext& uiContext, InputManager& inputManager, CursorState& cursorState);
        ~Controller();
        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;

        void enter();
        void leave();
        bool active() const;
        bool blocksGameInput() const;
        bool handleEvent(const SDL_Event& event);
        void finishEvents();
        bool draw(Drawing::RenderTarget& rt);
        void presentOpenGL();

    private:
        struct Session;
        IUiContext& _uiContext;
        InputManager& _inputManager;
        CursorState& _cursorState;
        std::unique_ptr<Session> _session;
        int32_t _savedMouseX{}, _savedMouseY{};
        bool _savedRelativeMouseMode{};
        int _savedCursorVisibility{};
        bool _restoreInput{};
    };

    // Implemented by the UI host; the controller's lifetime is tied to that host, not a global singleton.
    Controller& GetController();

    inline void Enter()
    {
        GetController().enter();
    }
    inline bool IsActive()
    {
        return GetController().active();
    }
    inline bool BlocksGameInput()
    {
        return GetController().blocksGameInput();
    }
} // namespace OpenRCT2::Ui::View3D
