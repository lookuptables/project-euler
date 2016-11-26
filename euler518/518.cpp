#include <cmath>

#include <thread>

#include "lib/primes.h"

void solve(u32 n) {
    Erato erato(n);
    u64 result = 0;

    static constexpr int kThreads = 4;

    vector<thread> threads(kThreads);
    for (size_t iThread = 0; iThread < kThreads; ++iThread) {
        threads[iThread] = thread( [&, iThread] {
            u64 localResult = 0;
            for (size_t i = iThread; i < erato.primes_.size(); i += kThreads) {
                if (0 == (i % 1000)) {
                    cerr << "..." << i << endl;
                }
                for (size_t j = i + 1; j < erato.primes_.size(); ++j) {
                    double alpha = static_cast<double>(erato.primes_[j] + 1)/(erato.primes_[i] + 1);
                    u32 c = static_cast<u32>((erato.primes_[j] + 1)*alpha + 0.4999);
                    if (c >= n) {
                        break;
                    }
                    if (fabs( (erato.primes_[j] + 1)*alpha - c) < 0.001 ) {
                        if (erato.isPrime(c - 1)) {
                            localResult += erato.primes_[i];
                            localResult += erato.primes_[j];
                            localResult += c - 1;
                            // cerr << erato.primes_[i] << " " << erato.primes_[j] << " " << (c - 1) << endl;
                        }
                    }
                }
            }
            result += localResult;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    cout << n << " " << result << endl;
}

int main() {
    solve(100);
    solve(100000000);

    return 0;
}
