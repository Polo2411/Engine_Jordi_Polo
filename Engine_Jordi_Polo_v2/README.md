# 3D Game Engine — Assignment 2 (AAA Video Games Master)

**Repository (GitHub):** https://github.com/Polo2411/Engine_Jordi_Polo

## Short description
This project is a lightweight **DirectX 12 3D engine** developed for the **AAA Video Games Master** course **“3D Game Engine”** (Assignment 2).  
It loads and renders a **glTF duck model** using **Phong shading with PBR-style controls**, and includes an editor layer built with **ImGui**.  
The scene is rendered into a **RenderTexture** and shown inside an ImGui **Scene** window. A **grid** and **axis** can be enabled to help with orientation.  
A transform **gizmo (ImGuizmo)** is included to control **translation / rotation / scale**, and the same transform can also be edited through ImGui controls.

## How to use the engine (controls)
The engine keeps **Unity-like camera controls**.

### Camera controls
- **Right Mouse Button (hold) + Mouse move:** Free look / rotate camera.
- **W / A / S / D:** Move the camera (as implemented in the engine).
- **Mouse Wheel:** Zoom in / out.
- **CTRL + Left Mouse Button (hold) + Mouse move:** Orbit around the focus/target (Unity-like orbit).
- **F:** Focus the camera on the model.
- **Shift (hold):** Faster movement.

## Editor (ImGui) windows
You can move and resize the ImGui windows to organize the layout (Scene on the left, Options on the right by default).

### Scene window
- Displays the rendered scene (duck + optional grid/axis).
- Shows the transform gizmo overlay when enabled.

### Geometry Viewer Options window
Contains all required assignment controls:

#### General
- **Show grid:** Toggle the ground grid.
- **Show axis:** Toggle the axis triad.
- **Show guizmo:** Toggle ImGuizmo display.

#### Transform editing
- **Translate / Rotate / Scale:** Select the operation mode.
- **Tr / Rt / Sc numeric controls:** Edit translation, rotation and scale values directly from ImGui.

#### Light parameters
- **Light Direction:** Directional light vector (with a Normalize button).
- **Light Colour:** RGB light color.
- **Ambient Colour:** RGB ambient light contribution.

#### Material parameters (Phong)
- **Diffuse Colour (Cd):** Base diffuse color.
- **Specular Colour (F0):** Specular reflectance color.
- **Shininess (n):** Specular exponent.
- **Use Texture:** Enable/disable the diffuse texture when available.

## Additional functionality (beyond minimum requirements)
- The scene is rendered to an off-screen **RenderTexture** and displayed inside ImGui.
- Runtime editing of **light** and **material** parameters to iterate quickly.
- Optional **debug grid and axis** for easier scene orientation.

## Comments for teachers
- **Input focus:** Camera controls may feel inactive if the user is currently interacting with an ImGui widget (e.g., dragging a slider or editing a numeric field). Clicking outside the active widget restores the expected camera input behavior.
- **Transform interaction:** The model transform is fully editable via the **ImGui numeric controls** (Tr/Rt/Sc). The gizmo overlay is displayed in the Scene view; interaction may depend on window focus and ImGui input capture in the current layout, so the numeric controls are the reliable way to edit the transform during evaluation.

