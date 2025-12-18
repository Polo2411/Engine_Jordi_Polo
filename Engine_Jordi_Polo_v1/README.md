# Engine Jordi Polo

## Description
Engine Jordi Polo is an educational real-time rendering engine developed in C++ using DirectX 12 as part of the UPC School AAA Video Games Master.

The engine focuses on low-level graphics programming concepts such as GPU resource management, texture sampling, mipmapping, camera systems, and UI integration using Dear ImGui.

GitHub repository:
https://github.com/Polo2411/Engine_Jordi_Polo.git

---

## How to Use the Engine

### Camera Controls (Unity-like)

> ⚠️ Note:  
> When the engine starts, the ImGui window is focused by default.  
> To control the camera, **left-click anywhere on the main render window first** to remove focus from ImGui.

- **Right Mouse Button + Mouse Move**  
  Free look around (FPS-style camera)

- **Right Mouse Button + WASD**  
  Move the camera forward, backward, and sideways

- **Mouse Wheel**  
  Zoom in and out (dolly movement)

- **Alt + Left Mouse Button**  
  Orbit around the object at the origin

- **F key**  
  Focus the camera on the rendered geometry

- **Shift (hold)**  
  Increases movement speed

---

## User Interface (ImGui)

The engine includes an ImGui window that provides:

- Frames Per Second (FPS) counter
- Average frame time in milliseconds
- Texture sampler configuration:
  - Wrap + Bilinear filtering
  - Clamp + Bilinear filtering
  - Wrap + Point filtering
  - Clamp + Point filtering

These options allow real-time visualization of texture sampling differences.

---

## Rendering Features

- A textured 3D quad rendered at the origin of the world coordinate system
- Grid and axis rendered at the origin to facilitate spatial orientation
- Aspect ratio–correct projection (no deformation on window resize)
- Mipmapped textures with proper mip filtering when using bilinear sampling
- Runtime texture sampler switching via ImGui

---

## Additional Functionality (Beyond Assignment Requirements)

- Modular engine architecture with independent systems (camera, resources, samplers, descriptors)
- Centralized resource loading system for buffers and textures
- Descriptor heap management for SRVs and samplers
- Frame-rate–independent camera movement
- Debug drawing system for grid and axis visualization
- Automatic GPU synchronization for resource uploads

---

## Notes for Teachers

- The engine uses DirectX 12 and Dear ImGui
- Textures are loaded at startup and can be either DDS or standard image formats
- Mipmaps are supported and correctly sampled when bilinear filtering is enabled
- Camera controls are inspired by Unity’s Scene View behavior

---
