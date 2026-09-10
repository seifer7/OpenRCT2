# Copilot Instructions

## Project Guidelines
- For the OpenRCT2 fork, keep the vast majority of the 3D view implementation separate from upstream project files and minimize integration changes to make syncing with upstream OpenRCT2 easier.
- Keep the OpenRCT2 fork's view3d implementation in a dedicated module with minimal thin hooks in upstream files, isolating controllers, rendering, camera, map access, and presentation to ease upstream syncing.
- For OpenRCT2's 3D view, keep the mouse cursor visible and rotate the camera only while the right mouse button is held.
- For OpenRCT2's 3D view, reuse the game's existing sprites for everything by default, unless the user explicitly specifies otherwise.
