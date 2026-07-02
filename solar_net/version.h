#pragma once

#include <string_view>

namespace solar_net {

struct Version {
    static constexpr int kMajor = 0;
    static constexpr int kMinor = 1;
    static constexpr int kPatch = 0;

    [[nodiscard]] static constexpr std::string_view ProjectName() noexcept {
        return "SolarNet";
    }

    [[nodiscard]] static constexpr std::string_view VersionString() noexcept {
        return "0.1.0";
    }
};

} // namespace solar_net
