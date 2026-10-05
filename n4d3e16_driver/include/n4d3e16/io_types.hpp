#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace n4d3e16 {
using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

enum class Status {
    ok, invalid_argument, not_connected, not_configured, busy,
    timeout, io_error, device_exception
};

struct Result {
    Status status = Status::ok;
    std::string message;
    uint8_t exception_code = 0;
    bool execution_unknown = false; // Sent/partially sent, but no valid confirmation.
    explicit operator bool() const { return status == Status::ok; }
};

struct InputState {
    uint16_t active_mask = 0; // bit 0 = IN1; 1 means active, NOT electrical high.
    uint16_t changed_mask = 0;
    bool valid = false;
    bool first_sample = false;
    Clock::time_point received_at{};
};

struct DriverConfig {
    uint8_t slave_id = 1; // Physical A0-A5 DIP address: use 1..63.
    unsigned baud = 9600;
    Milliseconds response_timeout{250}; // Initial setting, not a measured guarantee.
};

struct ChannelCommand {
    uint8_t channel;
    uint16_t value; // Native command encoding, e.g. 0x0100 / 0x0200.
};
} // namespace n4d3e16
