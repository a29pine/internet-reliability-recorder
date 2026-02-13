#include <cassert>

#include "../src/report/report_gen.hpp"

int main() {
    irr::ReportStats stats;
    // Should fail gracefully if file missing
    bool ok = !irr::generate_report("/nonexistent", "/tmp/irr_dummy.html", stats);
    assert(ok);
    return 0;
}
