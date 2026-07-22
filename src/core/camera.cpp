#include "camera.hpp"
#include <cmath>
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

Mat4 perspective(float fov_y, float aspect, float znear, float zfar) {
    float t = 1.0f / std::tan(fov_y * 0.5f);
    Mat4 P;
    P.m[0]  = t / aspect;
    P.m[5]  = t;
    P.m[10] = zfar / (zfar - znear);          // LH
    P.m[11] = 1.0f;
    P.m[14] = -(zfar * znear) / (zfar - znear);
    return P;
}

static void mul(const Mat4& A, const Mat4& B, Mat4& out) { // out = A*B (col-major)
    for (int c=0;c<4;++c) for (int r=0;r<4;++r) {
        float s=0; for (int k=0;k<4;++k) s += A.m[k*4+r]*B.m[c*4+k];
        out.m[c*4+r]=s;
    }
}

ScreenPoint world_to_screen(Vec3 world, const CameraState& cam,
                            float screen_w, float screen_h) {
    Mat4 V = look_at(cam.position, cam.front, {0,1,0});
    Mat4 P = perspective(cam.fov_y, screen_w/screen_h, 0.05f, 10000.0f);
    Mat4 VP; mul(P, V, VP);
    float cx = VP.m[0]*world.x + VP.m[4]*world.y + VP.m[8]*world.z  + VP.m[12];
    float cy = VP.m[1]*world.x + VP.m[5]*world.y + VP.m[9]*world.z  + VP.m[13];
    float cw = VP.m[3]*world.x + VP.m[7]*world.y + VP.m[11]*world.z + VP.m[15];
    ScreenPoint sp;
    if (cw <= 0.0001f) { sp.behind = true; return sp; }
    float ndcx = cx / cw, ndcy = cy / cw;
    sp.x = (ndcx * 0.5f + 0.5f) * screen_w;
    sp.y = (1.0f - (ndcy * 0.5f + 0.5f)) * screen_h;   // y-down for screen
    sp.on_screen = (ndcx>=-1 && ndcx<=1 && ndcy>=-1 && ndcy<=1);
    return sp;
}
}
