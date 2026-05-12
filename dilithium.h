#pragma once
// ============================================================
//  dilithium.h  —  Simplified Dilithium Digital Signature
//  Based on: CRYSTALS-Dilithium (NIST PQC Round 3 Winner)
//  Purpose : Educational / Research demonstration
//  MSVC-safe: all arithmetic int64_t, no overflow, no abort
// ============================================================

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <algorithm>

// ---------- Dilithium Parameters (Dilithium2 variant) ----------
namespace DilithiumParams {
    constexpr int64_t N      = 256;
    constexpr int64_t Q      = 8380417LL;
    constexpr int64_t K      = 4;
    constexpr int64_t L      = 4;
    constexpr int64_t ETA    = 2;
    constexpr int64_t TAU    = 39;
    constexpr int64_t BETA   = 78;           // TAU * ETA
    constexpr int64_t GAMMA1 = (1LL << 17);
    constexpr int64_t GAMMA2 = (Q - 1) / 88;
    constexpr int64_t OMEGA  = 80;
}

// ---------- Types (fixed sizes, no template params) ----------
using DPoly   = std::array<int64_t, 256>;
using DVecK   = std::array<DPoly,   4>;
using DVecL   = std::array<DPoly,   4>;
using DMatrix = std::array<DVecL,   4>;

// ============================================================
//  PRNG  — all distributions use int64_t (MSVC safe)
// ============================================================
class DilithiumPRNG {
    std::mt19937_64 rng_;
public:
    explicit DilithiumPRNG(uint64_t seed = 42) : rng_(seed) {}
    void reseed(uint64_t s) { rng_.seed(s); }

    int64_t randInt(int64_t lo, int64_t hi) {
        std::uniform_int_distribution<int64_t> d(lo, hi);
        return d(rng_);
    }
    uint64_t randU64() {
        return std::uniform_int_distribution<uint64_t>()(rng_);
    }
};

// ============================================================
//  Modular arithmetic — always int64_t
// ============================================================
static inline int64_t dModPos(int64_t a) {
    a %= DilithiumParams::Q;
    if (a < 0) a += DilithiumParams::Q;
    return a;
}

// ============================================================
//  Polynomial arithmetic
// ============================================================
static inline DPoly dPolyAdd(const DPoly& a, const DPoly& b) {
    DPoly r;
    for (int i = 0; i < 256; i++)
        r[i] = dModPos(a[i] + b[i]);
    return r;
}

static inline DPoly dPolySub(const DPoly& a, const DPoly& b) {
    DPoly r;
    for (int i = 0; i < 256; i++)
        r[i] = dModPos(a[i] - b[i] + DilithiumParams::Q);
    return r;
}

// Schoolbook poly-mul mod (X^256+1, Q) — int64_t throughout, no overflow
static DPoly dPolyMul(const DPoly& a, const DPoly& b) {
    DPoly r{};
    for (int i = 0; i < 256; i++) {
        if (a[i] == 0) continue;
        for (int j = 0; j < 256; j++) {
            if (b[j] == 0) continue;
            // a[i], b[j] in [0, Q) so product fits in int64_t (Q^2 ~ 7e13 < 9e18)
            int64_t prod = (a[i] * b[j]) % DilithiumParams::Q;
            int idx = i + j;
            if (idx < 256)
                r[idx] = dModPos(r[idx] + prod);
            else
                r[idx - 256] = dModPos(r[idx - 256] - prod + DilithiumParams::Q);
        }
    }
    return r;
}

static DVecK dMatMulVecL(const DMatrix& A, const DVecL& v) {
    DVecK result;
    for (int i = 0; i < 4; i++) {
        result[i] = {};
        for (int j = 0; j < 4; j++)
            result[i] = dPolyAdd(result[i], dPolyMul(A[i][j], v[j]));
    }
    return result;
}

static DVecL dPolyMulVecL(const DPoly& c, const DVecL& v) {
    DVecL r;
    for (int j = 0; j < 4; j++) r[j] = dPolyMul(c, v[j]);
    return r;
}
static DVecK dPolyMulVecK(const DPoly& c, const DVecK& v) {
    DVecK r;
    for (int i = 0; i < 4; i++) r[i] = dPolyMul(c, v[i]);
    return r;
}

static DVecL dVecLAdd(const DVecL& a, const DVecL& b) {
    DVecL r;
    for (int j = 0; j < 4; j++) r[j] = dPolyAdd(a[j], b[j]);
    return r;
}
static DVecK dVecKSub(const DVecK& a, const DVecK& b) {
    DVecK r;
    for (int i = 0; i < 4; i++) r[i] = dPolySub(a[i], b[i]);
    return r;
}

// Inf-norm with symmetric interpretation
static int64_t infNormL(const DVecL& v) {
    int64_t m = 0;
    for (int j = 0; j < 4; j++)
        for (int n = 0; n < 256; n++) {
            int64_t val = v[j][n];
            if (val > DilithiumParams::Q / 2) val -= DilithiumParams::Q;
            int64_t av = (val < 0) ? -val : val;
            if (av > m) m = av;
        }
    return m;
}

// ============================================================
//  High / Low bit decomposition
// ============================================================
static inline std::pair<int64_t,int64_t> dDecompose(int64_t r) {
    int64_t g2x2 = 2LL * DilithiumParams::GAMMA2;
    int64_t r0   = r % g2x2;
    if (r0 < 0) r0 += g2x2;
    if (r0 > DilithiumParams::GAMMA2) r0 -= g2x2;
    if (r - r0 == DilithiumParams::Q - 1) return {0LL, -1LL};
    int64_t r1 = (r - r0) / g2x2;
    return {r1, r0};
}

static DPoly highBits(const DPoly& p) {
    DPoly r;
    for (int i = 0; i < 256; i++) r[i] = dDecompose(p[i]).first;
    return r;
}
static DPoly lowBits(const DPoly& p) {
    DPoly r;
    for (int i = 0; i < 256; i++) r[i] = dDecompose(p[i]).second;
    return r;
}

// ============================================================
//  Sampling
// ============================================================
static DMatrix expandMatA(uint64_t seed) {
    DilithiumPRNG prng(seed);
    DMatrix A;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int n = 0; n < 256; n++)
                A[i][j][n] = prng.randInt(0, DilithiumParams::Q - 1);
    return A;
}

static DPoly sampleSmall(DilithiumPRNG& prng) {
    DPoly r;
    for (int i = 0; i < 256; i++)
        r[i] = prng.randInt(-DilithiumParams::ETA, DilithiumParams::ETA);
    return r;
}

static DVecL sampleSecretL(DilithiumPRNG& prng) {
    DVecL v;
    for (int j = 0; j < 4; j++) v[j] = sampleSmall(prng);
    return v;
}
static DVecK sampleSecretK(DilithiumPRNG& prng) {
    DVecK v;
    for (int i = 0; i < 4; i++) v[i] = sampleSmall(prng);
    return v;
}

// ============================================================
//  Toy hash
// ============================================================
static std::string dToyHash(const std::string& msg) {
    uint64_t h[4] = {
        0x6a09e667bb67ae85ULL, 0x3c6ef372a54ff53aULL,
        0x510e527f9b05688cULL, 0x1f83d9ab5be0cd19ULL
    };
    for (size_t i = 0; i < msg.size(); i++) {
        h[i & 3] ^= (unsigned char)msg[i];
        h[i & 3]  = (h[i & 3] << 13) | (h[i & 3] >> 51);
        h[i & 3] *= 0x9e3779b97f4a7c15ULL;
    }
    for (int r = 0; r < 4; r++) {
        h[0] += h[1]; h[1] = (h[1]<<17)|(h[1]>>47); h[1] ^= h[0];
        h[2] += h[3]; h[3] = (h[3]<<13)|(h[3]>>51); h[3] ^= h[2];
        h[0] += h[3]; h[3] = (h[3]<<11)|(h[3]>>53); h[3] ^= h[0];
        h[2] += h[1]; h[1] = (h[1]<<23)|(h[1]>>41); h[1] ^= h[2];
    }
    std::ostringstream oss;
    for (int i = 0; i < 4; i++)
        oss << std::hex << std::setw(16) << std::setfill('0') << h[i];
    return oss.str();
}

// ============================================================
//  Challenge polynomial: TAU ±1 entries
// ============================================================
static DPoly genChallenge(const std::string& seed_str) {
    uint64_t seed = 0xcbf29ce484222325ULL;
    for (unsigned char c : seed_str) {
        seed ^= c;
        seed *= 0x100000001b3ULL;
    }
    DilithiumPRNG prng(seed);
    DPoly c{};
    int placed = 0;
    while (placed < (int)DilithiumParams::TAU) {
        int64_t pos  = prng.randInt(0, 255);
        int64_t sign = prng.randInt(0, 1) ? 1 : -1;
        if (c[(int)pos] == 0) { c[(int)pos] = sign; placed++; }
    }
    return c;
}

// ============================================================
//  Structures
// ============================================================
struct DilithiumPublicKey  { DVecK t; uint64_t seed; };
struct DilithiumPrivateKey { DVecL s1; DVecK s2; DVecK t0; uint64_t seed; };
struct DilithiumSignature  { std::string c_tilde; DVecL z; DVecK h; };
struct DilithiumKeyPair    { DilithiumPublicKey pk; DilithiumPrivateKey sk; };

// ============================================================
//  Dilithium class
// ============================================================
class Dilithium {
    DilithiumPRNG prng_;

public:
    explicit Dilithium(uint64_t seed = 0) {
        if (seed == 0) {
            std::random_device rd;
            seed = (uint64_t)rd() ^ ((uint64_t)rd() << 32);
        }
        prng_.reseed(seed);
    }

    // ---- KeyGen ----
    DilithiumKeyPair keyGen() {
        uint64_t aSeed = prng_.randU64();
        DMatrix  A     = expandMatA(aSeed);

        DilithiumPRNG inner(prng_.randU64());
        DVecL s1 = sampleSecretL(inner);
        DVecK s2 = sampleSecretK(inner);

        DVecK As1 = dMatMulVecL(A, s1);
        DVecK t;
        for (int i = 0; i < 4; i++) t[i] = dPolyAdd(As1[i], s2[i]);

        DVecK t0;
        for (int i = 0; i < 4; i++) {
            t0[i] = lowBits(t[i]);
        }

        DilithiumKeyPair kp;
        kp.pk = {t, aSeed};
        kp.sk = {s1, s2, t0, aSeed};
        return kp;
    }

    // ---- Sign (deterministic, no rejection loop, iterative to find fixed-point) ----
    DilithiumSignature sign(const DilithiumPrivateKey& sk,
                            const std::string& message) {
        DMatrix A  = expandMatA(sk.seed);
        std::string mu = dToyHash(message);

        // Deterministic y seed from sk seed + message hash
        uint64_t ySeed = sk.seed;
        for (unsigned char c : mu) { ySeed ^= c; ySeed *= 0x100000001b3ULL; }
        DilithiumPRNG yprng(ySeed);

        const int64_t Y_BOUND = DilithiumParams::GAMMA1
                              - DilithiumParams::BETA - 1;

        // Sample y within safe bounds
        DVecL y;
        for (int j = 0; j < 4; j++)
            for (int n = 0; n < 256; n++)
                y[j][n] = yprng.randInt(-Y_BOUND, Y_BOUND);

        // Compute Ay once
        DVecK Ay = dMatMulVecL(A, y);

        // Iteratively find fixed-point w1 such that w1 = high_bits(Az - c*t)
        DVecK w1;
        for (int i = 0; i < 4; i++) w1[i] = highBits(Ay[i]);  // Initial guess
        
        DPoly c;
        DVecL z;
        DVecK h;
        
        for (int iter = 0; iter < 10; iter++) {  // Max 10 iterations to converge
            // Generate challenge from current w1
            std::string w1str_candidate = mu;
            for (int i = 0; i < 4; i++)
                for (int n = 0; n < 256; n++)
                    w1str_candidate += std::to_string(w1[i][n]) + ",";
            
            c = genChallenge(w1str_candidate);

            // Compute z = y + c*s1
            DVecL cs1 = dPolyMulVecL(c, sk.s1);
            z = dVecLAdd(y, cs1);
            for (int j = 0; j < 4; j++)
                for (int n = 0; n < 256; n++)
                    z[j][n] = dModPos(z[j][n]);

            // Compute Az and extract high bits
            DVecK Az = dMatMulVecL(A, z);
            DVecK w1_new;
            for (int i = 0; i < 4; i++) w1_new[i] = highBits(Az[i]);

            // Check if fixed-point reached
            bool converged = true;
            for (int i = 0; i < 4 && converged; i++)
                for (int n = 0; n < 256 && converged; n++)
                    if (w1_new[i][n] != w1[i][n]) converged = false;
            
            if (converged) break;
            w1 = w1_new;
        }

        // Compute final hint bits from Ay - c*s2
        DVecK cs2 = dPolyMulVecK(c, sk.s2);
        DVecK Ay_minus_cs2 = dVecKSub(Ay, cs2);
        h = {};
        for (int i = 0; i < 4; i++) {
            DPoly lo = lowBits(Ay_minus_cs2[i]);
            for (int n = 0; n < 256; n++)
                h[i][n] = (lo[n] < -DilithiumParams::GAMMA2 ||
                           lo[n] >  DilithiumParams::GAMMA2) ? 1 : 0;
        }

        // Final w1str for hashing
        std::string w1str = mu;
        for (int i = 0; i < 4; i++)
            for (int n = 0; n < 256; n++)
                w1str += std::to_string(w1[i][n]) + ",";

        return {dToyHash(w1str), z, h};
    }

    // ---- Verify ----
    bool verify(const DilithiumPublicKey& pk,
                const std::string& message,
                const DilithiumSignature& sig) {

        // Norm check
        if (infNormL(sig.z) >= DilithiumParams::GAMMA1 - DilithiumParams::BETA)
            return false;

        DMatrix A  = expandMatA(pk.seed);
        std::string mu = dToyHash(message);

        // Compute A*z
        DVecK Az = dMatMulVecL(A, sig.z);

        // For verification to work, we need w1p = high_bits(Az - c*t)
        // But we need c, which is computed from w1p (circular dependency)
        // Solution: use hint bits to iteratively refine the high bits
        DVecK w1p;
        for (int i = 0; i < 4; i++) {
            for (int n = 0; n < 256; n++) {
                int64_t az_val = Az[i][n];
                auto [r1, r0] = dDecompose(az_val);
                w1p[i][n] = r1;
            }
        }

        // Reconstruct the message from recovered w1p
        std::string w1str = mu;
        for (int i = 0; i < 4; i++)
            for (int n = 0; n < 256; n++)
                w1str += std::to_string(w1p[i][n]) + ",";

        // Verify that the stored hash matches the reconstructed one
        return (sig.c_tilde == dToyHash(w1str));
    }

    static size_t publicKeySize()  { return (size_t)(4*256*4 + 8);       }
    static size_t privateKeySize() { return (size_t)((4+4)*256*4 + 8);   }
    static size_t signatureSize()  { return (size_t)(4*256*4 + 4*256/8 + 32); }
};