#pragma once
#include "Globals.h"

void ExtractFrustumPlanes(Vector4 outPlanes[6], const Matrix& viewProjection, bool normalize = false);
