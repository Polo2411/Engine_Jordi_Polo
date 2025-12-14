#pragma once
#include "Globals.h"

// Extrae los 6 planos del frustum desde la matriz ViewProjection.
// Orden típico: Left, Right, Top, Bottom, Near, Far
void ExtractFrustumPlanes(Vector4 outPlanes[6], const Matrix& viewProjection, bool normalize = false);
