#include "Globals.h"

// TinyGLTF implementation TU (compile ONCE)
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// IMPORTANT: do NOT define TINYGLTF_NO_STB_IMAGE / WRITE here.
// We want stb image decoding enabled so glTF textures (png/jpg) work.

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267)

#include "tiny_gltf.h"

#pragma warning(pop)
