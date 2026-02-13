#include <fstream>
#include <string>

#include "../src/core/event_bus.hpp"
#include "../src/core/store_jsonl.hpp"

int main() {
    std::string path = "/tmp/irr_test_events.jsonl";
    {
        irr::JsonlStore store(path);
        irr::Event ev{"run",
                      123,
                      "2023-01-01T00:00:00Z",
                      "probe.tcp.connect",
                      "t",
                      "1.1.1.1",
                      "inet",
                      1000,
                      2000,
                      true,
                      12.3,
                      ""};
        store.on_event(ev);
    }
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    bool ok = line.find("probe.tcp.connect") != std::string::npos;
    return ok ? 0 : 1;
}
