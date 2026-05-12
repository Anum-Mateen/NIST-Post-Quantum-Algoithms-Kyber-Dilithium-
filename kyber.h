#pragma once
// ============================================================
//  kyber.h  —  Simplified Kyber KEM (Learning With Errors)
//  Based on: CRYSTALS-Kyber (NIST PQC Round 3 Winner)
//  Purpose : Educational / Research demonstration
//  Note    : This is a pedagogical implementation.
//            For production, use liboqs or PQClean.
// ============================================================

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>

// ---------- Kyber Parameters (Kyber-512 variant) ----------
namespace KyberParams {
    constexpr int N   = 256;   // Polynomial degree
    constexpr int Q   = 3329;  // Modulus
    constexpr int K   = 2;     // Module rank (Kyber-512)
    constexpr int ETA = 2;     // Noise parameter
    constexpr int DU  = 10;    // Compression param for ciphertext u
    constexpr int DV  = 4;     // Compression param for ciphertext v
    constexpr int SHARED_SECRET_BYTES = 32;
    constexpr int SEED_BYTES          = 32;
}

// ---------- Type Aliases ----------
using Poly     = std::array<int32_t, KyberParams::N>;
using PolyVec  = std::array<Poly, KyberParams::K>;

// ============================================================
//  Utility: simple PRNG wrapper (deterministic for demo)
// ============================================================
class SimplePRNG {
    std::mt19937 rng;
public:
    explicit SimplePRNG(uint64_t seed = 42) : rng(seed) {}
    void reseed(uint64_t s) { rng.seed(s); }

    int32_t randInt(int32_t lo, int32_t hi) {
        std::uniform_int_distribution<int32_t> dist(lo, hi);
        return dist(rng);
    }
    // Fill a byte array with pseudo-random bytes
    void randBytes(uint8_t* buf, size_t len) {
        std::uniform_int_distribution<int32_t> dist(0, 255);
        for (size_t i = 0; i < len; i++) buf[i] = static_cast<uint8_t>(dist(rng));
    }
};

// ============================================================
//  Polynomial Arithmetic (mod Q)
// ============================================================

// Reduce coefficient mod Q into [0, Q)
inline int32_t modQ(int32_t a) {
    a %= KyberParams::Q;
    if (a < 0) a += KyberParams::Q;
    return a;
}

// Add two polynomials mod Q
inline Poly polyAdd(const Poly& a, const Poly& b) {
    Poly r;
    for (int i = 0; i < KyberParams::N; i++)
        r[i] = modQ(a[i] + b[i]);
    return r;
}

// Subtract two polynomials mod Q
inline Poly polySub(const Poly& a, const Poly& b) {
    Poly r;
    for (int i = 0; i < KyberParams::N; i++)
        r[i] = modQ(a[i] - b[i] + KyberParams::Q);
    return r;
}

// Naive polynomial multiplication mod (X^N + 1, Q)
// For research demo: O(N^2). Real Kyber uses NTT.
Poly polyMul(const Poly& a, const Poly& b) {
    Poly r{};
    for (int i = 0; i < KyberParams::N; i++) {
        for (int j = 0; j < KyberParams::N; j++) {
            int idx = i + j;
            int32_t prod = modQ(a[i] * b[j]);
            if (idx < KyberParams::N)
                r[idx] = modQ(r[idx] + prod);
            else
                r[idx - KyberParams::N] = modQ(r[idx - KyberParams::N] - prod + KyberParams::Q);
        }
    }
    return r;
}

// PolyVec dot product -> single polynomial
inline Poly polyVecDot(const PolyVec& a, const PolyVec& b) {
    Poly r{};
    for (int k = 0; k < KyberParams::K; k++)
        r = polyAdd(r, polyMul(a[k], b[k]));
    return r;
}

// ============================================================
//  Noise & Seed Generation
// ============================================================

// Sample polynomial with small coefficients from CBD (centred binomial)
Poly sampleCBD(SimplePRNG& prng, int eta = KyberParams::ETA) {
    Poly r{};
    for (int i = 0; i < KyberParams::N; i++) {
        int32_t a = 0, b = 0;
        for (int j = 0; j < eta; j++) { a += prng.randInt(0,1); b += prng.randInt(0,1); }
        r[i] = modQ(a - b + KyberParams::Q);
    }
    return r;
}

// Sample a uniform polynomial in Rq
Poly sampleUniform(SimplePRNG& prng) {
    Poly r{};
    for (int i = 0; i < KyberParams::N; i++)
        r[i] = prng.randInt(0, KyberParams::Q - 1);
    return r;
}

// Build public matrix A (K×K) from a seed
using Matrix = std::array<PolyVec, KyberParams::K>;
Matrix expandA(uint64_t seed) {
    Matrix A;
    SimplePRNG prng(seed);
    for (int i = 0; i < KyberParams::K; i++)
        for (int j = 0; j < KyberParams::K; j++)
            A[i][j] = sampleUniform(prng);
    return A;
}

// ============================================================
//  Compression helpers (simulate Kyber compress/decompress)
// ============================================================
inline int32_t compress(int32_t x, int d) {
    // round(2^d / Q * x) mod 2^d
    int32_t t = ((1 << d) * x + KyberParams::Q / 2) / KyberParams::Q;
    return t & ((1 << d) - 1);
}

inline int32_t decompress(int32_t x, int d) {
    return (KyberParams::Q * x + (1 << (d-1))) / (1 << d);
}

Poly compressPoly(const Poly& p, int d) {
    Poly r;
    for (int i = 0; i < KyberParams::N; i++) r[i] = compress(p[i], d);
    return r;
}
Poly decompressPoly(const Poly& p, int d) {
    Poly r;
    for (int i = 0; i < KyberParams::N; i++) r[i] = decompress(p[i], d);
    return r;
}

// ============================================================
//  Key & Ciphertext Structures
// ============================================================
struct KyberPublicKey {
    PolyVec t;         // public vector t = As + e
    uint64_t seed;     // seed to reconstruct A
};

struct KyberPrivateKey {
    PolyVec s;         // secret vector
};

struct KyberCiphertext {
    PolyVec u;         // compressed u
    Poly    v;         // compressed v
};

struct KyberKeyPair {
    KyberPublicKey  pk;
    KyberPrivateKey sk;
};

// ============================================================
//  Kyber KEM
// ============================================================

// Hash helper (toy XOR-based hash for demo purposes)
inline uint64_t toyHash(const uint8_t* data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

// Encode a shared secret as hex string
std::string bytesToHex(const uint8_t* buf, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; i++)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
    return oss.str();
}

class Kyber {
    SimplePRNG prng_;
public:
    explicit Kyber(uint64_t seed = 0) {
        if (seed == 0) {
            std::random_device rd;
            seed = rd();
        }
        prng_.reseed(seed);
    }

    // ---- Key Generation ----
    KyberKeyPair keyGen() {
        // Random seed for matrix A
        uint64_t aSeed = prng_.randInt(0, INT32_MAX);
        Matrix A = expandA(aSeed);

        // Sample secret s and error e
        PolyVec s, e;
        for (int i = 0; i < KyberParams::K; i++) {
            s[i] = sampleCBD(prng_);
            e[i] = sampleCBD(prng_);
        }

        // t = A*s + e
        PolyVec t;
        for (int i = 0; i < KyberParams::K; i++) {
            Poly as{};
            for (int j = 0; j < KyberParams::K; j++)
                as = polyAdd(as, polyMul(A[i][j], s[j]));
            t[i] = polyAdd(as, e[i]);
        }

        KyberKeyPair kp;
        kp.pk = {t, aSeed};
        kp.sk = {s};
        return kp;
    }

    // ---- Encapsulation ----
    // Returns (ciphertext, shared_secret)
    std::pair<KyberCiphertext, std::string>
    encapsulate(const KyberPublicKey& pk) {
        Matrix A = expandA(pk.seed);

        // Sample r, e1, e2
        PolyVec r, e1;
        for (int i = 0; i < KyberParams::K; i++) {
            r[i]  = sampleCBD(prng_);
            e1[i] = sampleCBD(prng_);
        }
        Poly e2 = sampleCBD(prng_);

        // u = A^T * r + e1
        PolyVec u;
        for (int j = 0; j < KyberParams::K; j++) {
            Poly atr{};
            for (int i = 0; i < KyberParams::K; i++)
                atr = polyAdd(atr, polyMul(A[i][j], r[i]));
            u[j] = polyAdd(atr, e1[j]);
        }

        // Encode message as polynomial (all-ones for demo)
        Poly msg{};
        uint8_t msgBytes[KyberParams::SHARED_SECRET_BYTES];
        prng_.randBytes(msgBytes, KyberParams::SHARED_SECRET_BYTES);
        for (int i = 0; i < KyberParams::N && i < KyberParams::SHARED_SECRET_BYTES * 8; i++) {
            int bit = (msgBytes[i/8] >> (i%8)) & 1;
            msg[i] = bit ? (KyberParams::Q / 2) : 0;
        }

        // v = t^T * r + e2 + msg
        Poly v = polyVecDot(pk.t, r);
        v = polyAdd(v, e2);
        v = polyAdd(v, msg);

        // Compress
        KyberCiphertext ct;
        for (int i = 0; i < KyberParams::K; i++)
            ct.u[i] = compressPoly(u[i], KyberParams::DU);
        ct.v = compressPoly(v, KyberParams::DV);

        // Shared secret = hash of message bytes
        std::string ss = bytesToHex(msgBytes, KyberParams::SHARED_SECRET_BYTES);
        return {ct, ss};
    }

    // ---- Decapsulation ----
    std::string decapsulate(const KyberPrivateKey& sk, const KyberCiphertext& ct) {
        // Decompress
        PolyVec u;
        for (int i = 0; i < KyberParams::K; i++)
            u[i] = decompressPoly(ct.u[i], KyberParams::DU);
        Poly v = decompressPoly(ct.v, KyberParams::DV);

        // w = v - s^T * u
        Poly stu = polyVecDot(sk.s, u);
        Poly w   = polySub(v, stu);

        // Recover message bits
        uint8_t msgBytes[KyberParams::SHARED_SECRET_BYTES] = {};
        for (int i = 0; i < KyberParams::N && i < KyberParams::SHARED_SECRET_BYTES * 8; i++) {
            // Map back: if w[i] closer to Q/2 -> 1, else 0
            int32_t val = w[i];
            int32_t dist0 = std::min(val, KyberParams::Q - val);
            int32_t dist1 = std::min(std::abs(val - KyberParams::Q/2),
                                      KyberParams::Q - std::abs(val - KyberParams::Q/2));
            if (dist1 < dist0)
                msgBytes[i/8] |= (1 << (i%8));
        }

        return bytesToHex(msgBytes, KyberParams::SHARED_SECRET_BYTES);
    }

    // ---- Utility: key sizes in bytes (approx) ----
    static size_t publicKeySize()  { return KyberParams::K * KyberParams::N * 2 + 8; }
    static size_t privateKeySize() { return KyberParams::K * KyberParams::N * 2;     }
    static size_t ciphertextSize() {
        return KyberParams::K * KyberParams::N * KyberParams::DU / 8
             + KyberParams::N * KyberParams::DV / 8;
    }
};