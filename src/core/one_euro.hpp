#pragma once
// One Euro filter — adaptive low-pass for noisy 2D tracking. Smooths hard when
// the signal is slow (kills jitter) and lightly when fast (stays responsive).
// Pure, platform-independent; unit-tested natively. See Casiez et al. 2012.
namespace core {

struct LowPass {
    bool init = false;
    float y = 0.0f;
    // alpha in [0,1]: 1 = passthrough, smaller = smoother.
    float filter(float x, float alpha) {
        y = init ? alpha * x + (1.0f - alpha) * y : x;
        init = true;
        return y;
    }
    void reset() { init = false; }
};

struct OneEuro {
    float mincut = 1.2f;   // baseline cutoff (Hz): lower = smoother at rest
    float beta   = 0.04f;  // speed coefficient: higher = more responsive when fast
    float dcut   = 1.0f;   // cutoff for the derivative estimate

    LowPass xf, dxf;
    bool has = false;
    float xprev = 0.0f;

    static float alpha(float cutoff, float dt) {
        const float pi = 3.14159265358979f;
        float tau = 1.0f / (2.0f * pi * cutoff);
        return 1.0f / (1.0f + tau / dt);
    }

    void reset() { xf.reset(); dxf.reset(); has = false; }

    float filter(float x, float dt) {
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        float dx = has ? (x - xprev) / dt : 0.0f;
        xprev = x;
        has = true;
        float edx = dxf.filter(dx, alpha(dcut, dt));
        float speed = edx < 0 ? -edx : edx;
        float cutoff = mincut + beta * speed;
        return xf.filter(x, alpha(cutoff, dt));
    }
};

}  // namespace core
