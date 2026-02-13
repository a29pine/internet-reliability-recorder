#include <cassert>
#include <vector>
#include "../src/util/percentile.hpp"

int main() {
    using irr::percentile;
    std::vector<double> v{10, 20, 30, 40, 50};
    assert(percentile(v, 50) == 30);
    assert(percentile(v, 0) == 10);
    assert(percentile(v, 100) == 50);
    return 0;
}
