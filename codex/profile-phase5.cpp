// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone test of the actual zone gate, with a controllable sampler completion signal.
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "video_core/nx2_profile.h"

namespace {
std::atomic<bool> finished{};
}

namespace Common::Nx2SampleProfile {
bool Requested() {
    const char* value = std::getenv("EDEN_NX2_SAMPLE_PROFILE");
    return value != nullptr && std::strtoul(value, nullptr, 10) != 0;
}
bool Completed() {
    return finished.load(std::memory_order_acquire);
}
}

int main(int argc, char** argv) {
    namespace Profile = VideoCommon::Nx2Profile;
    if (argc != 3) {
        return 2;
    }
    const bool before = std::atoi(argv[1]) != 0;
    const bool after = std::atoi(argv[2]) != 0;
    if (Profile::Enabled() != before) {
        return 1;
    }
    std::thread sampler{[] { finished.store(true, std::memory_order_release); }};
    sampler.join();
    if (Profile::Enabled() != after) {
        return 1;
    }
    // A wait started before the second phase must not become a clock-epoch-sized drain.
    Profile::AddDrain(VideoCommon::DrainSite::ListWriters, 0);
    for (size_t i = 0; i < Profile::t_data.drain_ns.size(); ++i) {
        if (Profile::t_data.drain_ns[i] != 0 || Profile::t_data.drain_count[i] != 0) {
            return 1;
        }
    }
    std::cout << "zone gate " << before << " -> " << after << "; zero-start drain ignored\n";
}
