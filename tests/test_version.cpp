#include "solar_net/version.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace solar_net {
namespace {

TEST(VersionTest, ProjectNameIsSolarNet) {
    EXPECT_EQ(Version::ProjectName(), "SolarNet");
}

TEST(VersionTest, VersionStringIsCorrect) {
    EXPECT_EQ(Version::VersionString(), "0.1.0");
}

TEST(VersionTest, SemanticVersionMatches) {
    EXPECT_EQ(Version::kMajor, 0);
    EXPECT_EQ(Version::kMinor, 1);
    EXPECT_EQ(Version::kPatch, 0);
}

} // namespace
} // namespace solar_net
