// ============================================================
//  main.cpp  —  PQC Benchmark: Kyber KEM + Dilithium Signature
//  Authors  : Anum (Lead), Dania (Dev), Hafsa (Doc)
//  Purpose  : Measure and compare post-quantum crypto performance
//  Build    : g++ -O2 -std=c++17 main.cpp -o pqc_bench
//  Run      : ./pqc_bench
// ============================================================

#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <string>
#include <vector>
#include <numeric>
#include <cmath>
#include <sstream>

#include "kyber.h"
#include "dilithium.h"

// ============================================================
//  Timer helper (returns microseconds)
// ============================================================
using Clock  = std::chrono::high_resolution_clock;
using Micros = std::chrono::microseconds;

template<typename Fn>
double measureMicros(Fn fn, int reps = 1) {
    auto t0 = Clock::now();
    for (int i = 0; i < reps; i++) fn();
    auto t1 = Clock::now();
    return (double)std::chrono::duration_cast<Micros>(t1 - t0).count() / reps;
}

// ============================================================
//  Stats helpers
// ============================================================
double mean(const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}
double stddev(const std::vector<double>& v) {
    double m = mean(v);
    double sq = 0;
    for (double x : v) sq += (x - m) * (x - m);
    return std::sqrt(sq / v.size());
}
double vmin(const std::vector<double>& v) { return *std::min_element(v.begin(), v.end()); }
double vmax(const std::vector<double>& v) { return *std::max_element(v.begin(), v.end()); }

// ============================================================
//  Pretty print helpers
// ============================================================
void printSeparator(char c = '=', int w = 70) {
    std::cout << std::string(w, c) << "\n";
}
void printHeader(const std::string& title) {
    printSeparator();
    std::cout << "  " << title << "\n";
    printSeparator();
}

void printRow(const std::string& label, double val, const std::string& unit = "µs") {
    std::cout << std::left << std::setw(30) << label
              << std::right << std::setw(10) << std::fixed << std::setprecision(2)
              << val << " " << unit << "\n";
}

// ============================================================
//  CSV writer
// ============================================================
struct CSVRow {
    std::string algo;
    std::string operation;
    double mean_us, stddev_us, min_us, max_us;
    size_t key_size_bytes;
};

void writeCSV(const std::string& filename, const std::vector<CSVRow>& rows) {
    std::ofstream f(filename);
    f << "Algorithm,Operation,Mean_us,StdDev_us,Min_us,Max_us,KeySize_bytes\n";
    for (const auto& r : rows) {
        f << r.algo << ","
          << r.operation << ","
          << std::fixed << std::setprecision(3)
          << r.mean_us << ","
          << r.stddev_us << ","
          << r.min_us << ","
          << r.max_us << ","
          << r.key_size_bytes << "\n";
    }
    std::cout << "[CSV] Results saved to: " << filename << "\n";
}

// ============================================================
//  Kyber Benchmark
// ============================================================
std::vector<CSVRow> benchmarkKyber(int runs = 50, uint64_t seed = 0) {
    printHeader("KYBER KEM BENCHMARK  (runs=" + std::to_string(runs) + ")");

    Kyber kyber(seed);
    std::vector<double> kgTimes, encTimes, decTimes;

    // Pre-generate one keypair to avoid re-keying in enc/dec loops
    KyberKeyPair kp = kyber.keyGen();
    auto [ct, ss_enc] = kyber.encapsulate(kp.pk);

    for (int i = 0; i < runs; i++) {
        // KeyGen
        double kg = measureMicros([&]{ kyber.keyGen(); });
        kgTimes.push_back(kg);

        // Encapsulate
        double enc = measureMicros([&]{ kyber.encapsulate(kp.pk); });
        encTimes.push_back(enc);

        // Decapsulate
        double dec = measureMicros([&]{ kyber.decapsulate(kp.sk, ct); });
        decTimes.push_back(dec);
    }

    // --- Correctness check ---
    std::string ss_dec = kyber.decapsulate(kp.sk, ct);
    bool correct = (ss_enc == ss_dec);
    std::cout << "  Correctness (enc==dec): " << (correct ? "PASS ✓" : "FAIL ✗") << "\n";
    std::cout << "  Shared Secret (enc): " << ss_enc.substr(0,32) << "...\n";
    std::cout << "  Shared Secret (dec): " << ss_dec.substr(0,32) << "...\n\n";

    // --- Print stats ---
    std::cout << std::left << std::setw(30) << "Operation"
              << std::setw(12) << "Mean(µs)"
              << std::setw(12) << "StdDev(µs)"
              << std::setw(12) << "Min(µs)"
              << std::setw(12) << "Max(µs)" << "\n";
    printSeparator('-', 70);

    auto printStats = [&](const std::string& op, const std::vector<double>& v) {
        std::cout << std::left << std::setw(30) << op
                  << std::setw(12) << std::fixed << std::setprecision(2) << mean(v)
                  << std::setw(12) << stddev(v)
                  << std::setw(12) << vmin(v)
                  << std::setw(12) << vmax(v) << "\n";
    };
    printStats("KeyGen",       kgTimes);
    printStats("Encapsulation",encTimes);
    printStats("Decapsulation",decTimes);

    std::cout << "\n";
    std::cout << "  Public Key Size  : " << Kyber::publicKeySize()  << " bytes\n";
    std::cout << "  Private Key Size : " << Kyber::privateKeySize() << " bytes\n";
    std::cout << "  Ciphertext Size  : " << Kyber::ciphertextSize() << " bytes\n\n";

    // Build CSV rows
    std::vector<CSVRow> rows;
    rows.push_back({"Kyber", "KeyGen",        mean(kgTimes),  stddev(kgTimes),  vmin(kgTimes),  vmax(kgTimes),  Kyber::publicKeySize()});
    rows.push_back({"Kyber", "Encapsulate",   mean(encTimes), stddev(encTimes), vmin(encTimes), vmax(encTimes), Kyber::ciphertextSize()});
    rows.push_back({"Kyber", "Decapsulate",   mean(decTimes), stddev(decTimes), vmin(decTimes), vmax(decTimes), Kyber::privateKeySize()});
    return rows;
}

// ============================================================
//  Dilithium Benchmark
// ============================================================
std::vector<CSVRow> benchmarkDilithium(int runs = 50, uint64_t seed = 0) {
    printHeader("DILITHIUM SIGNATURE BENCHMARK  (runs=" + std::to_string(runs) + ")");

    Dilithium dil(seed);
    const std::string testMsg = "PQC Research Project 2026 — Kyber & Dilithium Comparison";

    std::vector<double> kgTimes, signTimes, verifyTimes;

    DilithiumKeyPair kp = dil.keyGen();
    DilithiumSignature sig = dil.sign(kp.sk, testMsg);

    for (int i = 0; i < runs; i++) {
        double kg = measureMicros([&]{ dil.keyGen(); });
        kgTimes.push_back(kg);

        double st = measureMicros([&]{ dil.sign(kp.sk, testMsg); });
        signTimes.push_back(st);

        double vt = measureMicros([&]{ dil.verify(kp.pk, testMsg, sig); });
        verifyTimes.push_back(vt);
    }

    // --- Correctness ---
    bool valid   = dil.verify(kp.pk, testMsg, sig);
    std::string tampered = testMsg + "TAMPERED";
    bool invalid = !dil.verify(kp.pk, tampered, sig);

    std::cout << "  Correctness (valid msg)   : " << (valid   ? "PASS ✓" : "FAIL ✗") << "\n";
    std::cout << "  Correctness (tampered msg): " << (invalid ? "PASS ✓" : "FAIL ✗") << "\n";
    std::cout << "  Challenge Hash: " << sig.c_tilde.substr(0,32) << "...\n\n";

    // --- Print stats ---
    std::cout << std::left << std::setw(30) << "Operation"
              << std::setw(12) << "Mean(µs)"
              << std::setw(12) << "StdDev(µs)"
              << std::setw(12) << "Min(µs)"
              << std::setw(12) << "Max(µs)" << "\n";
    printSeparator('-', 70);

    auto printStats = [&](const std::string& op, const std::vector<double>& v) {
        std::cout << std::left << std::setw(30) << op
                  << std::setw(12) << std::fixed << std::setprecision(2) << mean(v)
                  << std::setw(12) << stddev(v)
                  << std::setw(12) << vmin(v)
                  << std::setw(12) << vmax(v) << "\n";
    };
    printStats("KeyGen",  kgTimes);
    printStats("Sign",    signTimes);
    printStats("Verify",  verifyTimes);

    std::cout << "\n";
    std::cout << "  Public Key Size  : " << Dilithium::publicKeySize()  << " bytes\n";
    std::cout << "  Private Key Size : " << Dilithium::privateKeySize() << " bytes\n";
    std::cout << "  Signature Size   : " << Dilithium::signatureSize()  << " bytes\n\n";

    std::vector<CSVRow> rows;
    rows.push_back({"Dilithium", "KeyGen", mean(kgTimes),    stddev(kgTimes),    vmin(kgTimes),    vmax(kgTimes),    Dilithium::publicKeySize()});
    rows.push_back({"Dilithium", "Sign",   mean(signTimes),  stddev(signTimes),  vmin(signTimes),  vmax(signTimes),  Dilithium::signatureSize()});
    rows.push_back({"Dilithium", "Verify", mean(verifyTimes),stddev(verifyTimes),vmin(verifyTimes),vmax(verifyTimes),Dilithium::publicKeySize()});
    return rows;
}

// ============================================================
//  Comparison Summary
// ============================================================
void printComparison(const std::vector<CSVRow>& all) {
    printHeader("COMPARATIVE ANALYSIS SUMMARY");

    // Find rows by algo+op
    auto find = [&](const std::string& algo, const std::string& op) -> const CSVRow* {
        for (const auto& r : all)
            if (r.algo == algo && r.operation == op) return &r;
        return nullptr;
    };

    auto* kkg = find("Kyber",     "KeyGen");
    auto* dkg = find("Dilithium", "KeyGen");
    auto* kenc = find("Kyber",    "Encapsulate");
    auto* dsign = find("Dilithium","Sign");
    auto* kdec = find("Kyber",    "Decapsulate");
    auto* dver = find("Dilithium","Verify");

    std::cout << std::left << std::setw(25) << "Metric"
              << std::setw(20) << "Kyber (KEM)"
              << std::setw(20) << "Dilithium (Sig)" << "\n";
    printSeparator('-', 65);

    auto cmpRow = [&](const std::string& label,
                      const CSVRow* a, const CSVRow* b, bool keySize = false) {
        if (!a || !b) return;
        double va = keySize ? a->key_size_bytes : a->mean_us;
        double vb = keySize ? b->key_size_bytes : b->mean_us;
        std::string ua = keySize ? " bytes" : " µs";
        std::string ub = keySize ? " bytes" : " µs";
        std::cout << std::left << std::setw(25) << label
                  << std::setw(20) << (std::to_string((int)va) + ua)
                  << std::setw(20) << (std::to_string((int)vb) + ub) << "\n";
    };

    cmpRow("KeyGen Time",     kkg,  dkg);
    cmpRow("Enc/Sign Time",   kenc, dsign);
    cmpRow("Dec/Verify Time", kdec, dver);
    cmpRow("Public Key Size", kkg,  dkg, true);
    cmpRow("Ciphertext/Sig",  kenc, dsign, true);

    std::cout << "\n";
    printHeader("ANALYSIS NOTES (for report)");
    std::cout
      << "  1. Kyber (KEM) is designed for key exchange (confidentiality).\n"
      << "     Dilithium is designed for digital signatures (authenticity).\n\n"
      << "  2. Both are NIST-standardized post-quantum algorithms resistant\n"
      << "     to Shor's algorithm on quantum computers.\n\n"
      << "  3. Kyber is typically faster in encapsulation/decapsulation.\n"
      << "     Dilithium signing involves rejection sampling (variable time).\n\n"
      << "  4. Key sizes are larger than classical RSA/ECC but remain practical.\n\n"
      << "  5. This implementation uses naive O(N^2) polynomial multiplication.\n"
      << "     Production NTT-based implementations are 10-100x faster.\n\n";
}

// ============================================================
//  main
// ============================================================
int main(int argc, char* argv[]) {
    int runs = 25;
    if (argc > 1) runs = std::atoi(argv[1]);

    // Read optional seed from environment variable PQC_SEED.
    // server.py injects a fresh random seed on every /api/run call so
    // repeated runs produce genuinely different timing results.
    // If the env var is absent (manual run), fall back to std::random_device.
    uint64_t globalSeed = 0;
    const char* envSeed = std::getenv("PQC_SEED");
    if (envSeed && envSeed[0] != '\0') {
        globalSeed = (uint64_t)std::strtoull(envSeed, nullptr, 10);
    }
    if (globalSeed == 0) {
        std::random_device rd;
        globalSeed = ((uint64_t)rd() << 32) ^ (uint64_t)rd();
    }
    // Seed the global C rand() so any residual rand() calls are also varied
    std::srand((unsigned)globalSeed);

    std::cout << "\n";
    printSeparator('*');
    std::cout << "  PQC BENCHMARK: Kyber KEM vs Dilithium Digital Signature\n";
    std::cout << "  Project by: Anum (Lead), Dania (Dev), Hafsa (Doc)\n";
    std::cout << "  Algorithms : CRYSTALS-Kyber-512 + CRYSTALS-Dilithium2\n";
    std::cout << "  Runs       : " << runs << " iterations per operation\n";
    std::cout << "  Seed       : " << globalSeed << "\n";
    printSeparator('*');
    std::cout << "\n";

    std::vector<CSVRow> all;

    auto kyberRows     = benchmarkKyber(runs, globalSeed);
    auto dilithiumRows = benchmarkDilithium(runs, globalSeed ^ 0xDEADBEEFCAFEULL);

    all.insert(all.end(), kyberRows.begin(),     kyberRows.end());
    all.insert(all.end(), dilithiumRows.begin(), dilithiumRows.end());

    printComparison(all);
    writeCSV("results.csv", all);

    std::cout << "\n  Done. Open results.csv for graphs in Python/Excel.\n\n";
    return 0;
}