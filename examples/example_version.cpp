#include "solar_net/version.h"

#include <iostream>

int main() {
    std::cout << solar_net::Version::ProjectName() << " "
              << solar_net::Version::VersionString() << '\n';
    return 0;
}
