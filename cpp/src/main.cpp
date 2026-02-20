#include <iostream>
#include "headers/uhp.h"

int main() {
    // Disable synchronization between C and C++ standard streams for engine STDIO speed
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // Instantiate and trap process inside the communication loop
    Hive::UhpHandler uhp;
    uhp.loop();

    return 0;
}