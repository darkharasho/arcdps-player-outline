#include "camera.hpp"
namespace core {

Mat4 look_at(Vec3 eye, Vec3 front, Vec3 up) {
    Vec3 f = normalized(front);
    Vec3 r = normalized(cross(up, f));   // left-handed
    Vec3 u = cross(f, r);
    Mat4 M;
    // rows = basis; column-major storage
    M.m[0]=r.x; M.m[4]=r.y; M.m[8]=r.z;  M.m[12]=-dot(r,eye);
    M.m[1]=u.x; M.m[5]=u.y; M.m[9]=u.z;  M.m[13]=-dot(u,eye);
    M.m[2]=f.x; M.m[6]=f.y; M.m[10]=f.z; M.m[14]=-dot(f,eye);
    M.m[3]=0;   M.m[7]=0;   M.m[11]=0;   M.m[15]=1;
    return M;
}
}
