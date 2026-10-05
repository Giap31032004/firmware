#include "n4d3e16/n4d3e16_driver.hpp"
#include "n4d3e16/io_controller.hpp"
#include <csignal>
#include <iostream>
#include <vector>
#include <stdexcept>

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
unsigned number(const char* text, unsigned maximum) {
    std::size_t end = 0;
    const auto value = std::stoul(text, &end, 0);
    if (text[end] != '\0' || text[0] == '-' || value > maximum)
        throw std::invalid_argument("Invalid numeric argument");
    return static_cast<unsigned>(value);
}
bool check(const n4d3e16::Result& result) {
    if (result) return true;
    std::cerr << "IO error: " << result.message
              << (result.execution_unknown ? " (execution unknown)" : "") << '\n';
    return false;
}
}
int main(int argc, char** argv) {
    using namespace n4d3e16;
    if (argc < 4) {
        std::cerr << "Usage: amr_io_example DEVICE REPORT_SECONDS INPUT_MASK:CH[,CH...] ...\n"
                     "Masks use IN1..IN16; multiple rules combine shared outputs with OR.\n";
        return 2;
    }
    try {
        const auto report_seconds = static_cast<uint8_t>(number(argv[2], 255));
        std::vector<IoRule> rules;
        for (int i = 3; i < argc; ++i) {
            const std::string text(argv[i]);
            const auto colon = text.find(':');
            if (colon == std::string::npos)
                throw std::invalid_argument("Expected INPUT_MASK:CH[,CH...]");
            IoRule rule{static_cast<uint16_t>(number(text.substr(0, colon).c_str(), 0xFFFF)), {}};
            auto begin = colon + 1;
            for (;;) {
                const auto comma = text.find(',', begin);
                rule.outputs.push_back(static_cast<uint8_t>(number(
                    text.substr(begin, comma == std::string::npos ? comma : comma - begin).c_str(), 16)));
                if (comma == std::string::npos) break;
                begin = comma + 1;
            }
            rules.push_back(std::move(rule));
        }
        DriverConfig config{1, 9600, Milliseconds(250)};
        Driver io(std::make_unique<SerialPort>(argv[1], config.baud), config);
        IoController controller(std::move(rules), [&](uint8_t channel, bool active) {
            return active ? io.open(channel) : io.close(channel);
        });
        io.set_input_callback([&](const InputState& state) {
            controller.update_inputs(state.active_mask);
            if (state.first_sample || state.changed_mask)
                std::cout << "Input mask: 0x" << std::hex << state.active_mask << std::dec << '\n';
        });
        if (!check(io.connect()) || !check(io.configure_input_reporting(report_seconds))) return 1;
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        std::cout << "Input/output mapping active. Ctrl+C stops the example.\n";
        auto next_poll = Clock::now();
        while (running) {
            if (!check(io.process_once(Milliseconds(20)))) return 1;
            if (report_seconds == 0 && Clock::now() >= next_poll) {
                uint16_t inputs = 0;
                if (!check(io.read_inputs(inputs))) return 1;
                next_poll = Clock::now() + Milliseconds(100); // Example setting; tune later.
            }
            auto result = controller.apply();
            if (!result && result.status != Status::not_configured) {
                check(result); return 1;
            }
        }
        // No implicit close_all(): shutdown output policy is not yet specified.
        io.disconnect();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 2;
    }
    return 0;
}
