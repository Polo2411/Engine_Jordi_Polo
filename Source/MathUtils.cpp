#include "Globals.h"
#include "MathUtils.h"

void ExtractFrustumPlanes(Vector4 outPlanes[6], const Matrix& viewProjection, bool normalize)
{
    // OJO: en DirectX/SimpleMath es común transponer antes de extraer.
    Matrix vp = viewProjection;
    vp.Transpose();

    // Left
    outPlanes[0] = Vector4(vp._14 + vp._11, vp._24 + vp._21, vp._34 + vp._31, vp._44 + vp._41);
    // Right
    outPlanes[1] = Vector4(vp._14 - vp._11, vp._24 - vp._21, vp._34 - vp._31, vp._44 - vp._41);
    // Top
    outPlanes[2] = Vector4(vp._14 - vp._12, vp._24 - vp._22, vp._34 - vp._32, vp._44 - vp._42);
    // Bottom
    outPlanes[3] = Vector4(vp._14 + vp._12, vp._24 + vp._22, vp._34 + vp._32, vp._44 + vp._42);
    // Near
    outPlanes[4] = Vector4(vp._13, vp._23, vp._33, vp._43);
    // Far
    outPlanes[5] = Vector4(vp._14 - vp._13, vp._24 - vp._23, vp._34 - vp._33, vp._44 - vp._43);

    if (normalize)
    {
        for (int i = 0; i < 6; ++i)
        {
            Vector3 n(outPlanes[i].x, outPlanes[i].y, outPlanes[i].z);
            float len = n.Length();
            if (len > 1e-6f)
                outPlanes[i] /= len;
        }
    }
}
