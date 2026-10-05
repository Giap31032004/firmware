#include "n4d3e16/n4d3e16_driver.hpp"
#include <algorithm>
#include <stdexcept>

namespace n4d3e16 {
namespace {
bool valid_channel(uint8_t channel) { return channel >= 1 && channel <= 16; }
bool valid_native_command(uint16_t value) {
    const auto opcode = value >> 8;
    const auto parameter = value & 0xFF;
    return (opcode >= 1 && opcode <= 5 && parameter == 0) ||
           (opcode == 6 && parameter >= 1);
}
struct BusyGuard {
    bool& flag;
    explicit BusyGuard(bool& value) : flag(value) { flag = true; }
    ~BusyGuard() { flag = false; }
};
}
Driver::Driver(std::unique_ptr<Transport> transport, DriverConfig config)
    : transport_(std::move(transport)), config_(config) {
    if (!transport_ || config.slave_id < 1 || config.slave_id > 63 ||
        config.response_timeout.count() <= 0 ||
        (config.baud != 1200 && config.baud != 2400 && config.baud != 4800 &&
         config.baud != 9600 && config.baud != 19200))
        throw std::invalid_argument("Invalid driver configuration");
}
Driver::~Driver() { disconnect(); }
Result Driver::connect() {
    if (busy_) return {Status::busy, "Reentrant driver call"};
    if (connected_) return {Status::busy, "Already connected"};
    try {
        transport_->open(); parser_.clear(); inputs_ = {};
        connected_ = true; configured_ = false;
        return {};
    } catch (const std::exception& error) { return transport_failure(error); }
}
void Driver::disconnect() {
    transport_->close(); connected_ = false; configured_ = false;
    inputs_.valid = false; parser_.clear();
}
void Driver::set_input_callback(InputCallback callback) { on_input_ = std::move(callback); }
bool Driver::input_is_fresh(Milliseconds max_age) const {
    return connected_ && inputs_.valid && max_age.count() >= 0 &&
           Clock::now() - inputs_.received_at <= max_age;
}
Result Driver::transport_failure(const std::exception& error) {
    disconnect(); return {Status::io_error, error.what()};
}
void Driver::receive(Milliseconds wait) {
    uint8_t data[512];
    const auto size = transport_->read(data, sizeof(data), wait);
    if (size) parser_.feed(data, size);
}
void Driver::handle_input(const modbus::Bytes& frame) {
    if (frame[0] != config_.slave_id || frame[1] != 0x03 || frame[2] != 2) return;
    const auto mask = modbus::word(frame, 3);
    const bool first = !inputs_.valid;
    inputs_.changed_mask = first ? 0 : static_cast<uint16_t>(inputs_.active_mask ^ mask);
    inputs_.active_mask = mask;
    inputs_.first_sample = first; inputs_.valid = true;
    inputs_.received_at = Clock::now();
    if (on_input_) {
        // A user callback must not break a transaction after a command was sent.
        try { on_input_(inputs_); } catch (...) { ++callback_errors_; }
    }
}
Result Driver::process_once(Milliseconds wait) {
    if (!connected_) return {Status::not_connected, "Serial is closed"};
    if (busy_) return {Status::busy, "Reentrant driver call"};
    if (wait.count() < 0) return {Status::invalid_argument, "Negative wait"};
    BusyGuard guard(busy_);
    try {
        receive(wait);
        modbus::Bytes frame;
        while (parser_.pop(frame)) handle_input(frame);
        return {};
    } catch (const std::exception& error) { return transport_failure(error); }
}
Result Driver::transact(const modbus::Bytes& request, uint8_t expected_read_bytes,
                        modbus::Bytes& response) {
    if (!connected_) return {Status::not_connected, "Serial is closed"};
    if (busy_) return {Status::busy, "Reentrant driver call"};
    BusyGuard guard(busy_);
    bool write_attempted = false;
    try {
        // Observe a conservative 3.5 * 11-bit character gap before transmitting.
        // Keep input reports instead of flushing them. Bound the bus-idle wait.
        const auto gap = Milliseconds((38500u + config_.baud - 1) / config_.baud);
        const auto idle_deadline = Clock::now() + config_.response_timeout;
        for (;;) {
            uint8_t data[512];
            const auto left = std::chrono::duration_cast<Milliseconds>(idle_deadline - Clock::now());
            if (left < gap) return {Status::timeout, "Bus did not become idle; command not sent"};
            const auto size = transport_->read(data, sizeof(data), gap);
            if (!size) break;
            parser_.feed(data, size);
            modbus::Bytes stale;
            while (parser_.pop(stale)) handle_input(stale);
        }
        parser_.clear(); // Incomplete frame at an inter-frame silence is invalid.
        write_attempted = true;
        transport_->write(request.data(), request.size(), config_.response_timeout);
        const auto deadline = Clock::now() + config_.response_timeout;
        bool matched = false;
        Result result;
        for (;;) {
            modbus::Bytes frame;
            while (parser_.pop(frame)) {
                if (frame[0] != config_.slave_id) continue;
                handle_input(frame);
                if (matched) continue;
                if (frame[1] == static_cast<uint8_t>(request[1] | 0x80)) {
                    matched = true;
                    result = {Status::device_exception, "Modbus exception", frame[2]};
                } else if (frame[1] == request[1]) {
                    const bool read_match = frame[1] == 0x03 && frame[2] == expected_read_bytes;
                    const bool write_match = (frame[1] == 0x06 || frame[1] == 0x10) &&
                        std::equal(request.begin(), request.begin() + 6, frame.begin());
                    if (read_match || write_match) { matched = true; response = frame; }
                }
            }
            // Process all frames already received, including reports following ACK.
            if (matched) return result;
            const auto remaining = std::chrono::duration_cast<Milliseconds>(deadline - Clock::now());
            if (remaining.count() <= 0)
                return {Status::timeout, "No matching response; execution is unknown", 0, true};
            receive(remaining);
        }
    } catch (const std::exception& error) {
        auto result = transport_failure(error);
        result.execution_unknown = write_attempted;
        return result;
    }
}
Result Driver::write_register(uint16_t address, uint16_t value) {
    modbus::Bytes response;
    return transact(modbus::write_register(config_.slave_id, address, value), 0, response);
}
Result Driver::configure_input_reporting(uint8_t seconds) {
    if (busy_) return {Status::busy, "Reentrant driver call"};
    configured_ = false;
    // Remote receiver mode suppresses FC06 ACK on 0x0070, so disable it.
    const std::pair<uint16_t, uint16_t> settings[] = {
        {0x00FD, 0}, {0x00F9, 0}, {0x00FA, 0}, {0x00FC, 1}, {0x00FD, seconds}
    };
    for (const auto& setting : settings) {
        auto result = write_register(setting.first, setting.second);
        if (!result) return result;
    }
    configured_ = true;
    return {};
}
Result Driver::read_inputs(uint16_t& active_mask) {
    if (!configured_) return {Status::not_configured, "Configure bitmap reporting first"};
    modbus::Bytes response;
    auto result = transact(modbus::read_registers(config_.slave_id, 0x00C0, 1), 2, response);
    if (result) active_mask = modbus::word(response, 3);
    return result;
}
Result Driver::read_outputs(uint16_t& active_mask) {
    if (!configured_) return {Status::not_configured, "Configure bitmap reporting first"};
    modbus::Bytes response;
    // Read 16 output registers rather than bitmap 0x0070. A 32-byte read reply
    // can be distinguished from 2-byte unsolicited input bitmap reports.
    auto result = transact(modbus::read_registers(config_.slave_id, 0x0001, 16), 32, response);
    if (!result) return result;
    uint16_t mask = 0;
    for (unsigned i = 0; i < 16; ++i) {
        const auto value = modbus::word(response, 3 + 2 * i);
        if (value > 1) return {Status::io_error, "Unexpected output register value"};
        if (value) mask |= static_cast<uint16_t>(1u << i);
    }
    active_mask = mask;
    return {};
}
Result Driver::read_output(uint8_t channel, bool& active) {
    if (!valid_channel(channel)) return {Status::invalid_argument, "Channel must be 1..16"};
    uint16_t mask = 0;
    auto result = read_outputs(mask);
    if (result) active = (mask & (1u << (channel - 1))) != 0;
    return result;
}
Result Driver::channel_command(uint8_t channel, uint16_t value) {
    if (!valid_channel(channel)) return {Status::invalid_argument, "Channel must be 1..16"};
    return write_register(channel, value);
}
Result Driver::open(uint8_t channel) { return channel_command(channel, 0x0100); }
Result Driver::close(uint8_t channel) { return channel_command(channel, 0x0200); }
Result Driver::toggle(uint8_t channel) { return channel_command(channel, 0x0300); }
Result Driver::interlock(uint8_t channel) { return channel_command(channel, 0x0400); }
Result Driver::momentary(uint8_t channel) { return channel_command(channel, 0x0500); }
Result Driver::delay(uint8_t channel, unsigned seconds) {
    if (seconds < 1 || seconds > 255)
        return {Status::invalid_argument, "Delay must be 1..255 seconds"};
    return channel_command(channel, static_cast<uint16_t>(0x0600 | seconds));
}
Result Driver::open_all() { return write_register(0x0000, 0x0700); }
Result Driver::close_all() { return write_register(0x0000, 0x0800); }
Result Driver::write_outputs(uint16_t bitmap) { return write_register(0x0070, bitmap); }
Result Driver::write_channels(const std::vector<ChannelCommand>& commands) {
    if (commands.empty() || commands.size() > 16)
        return {Status::invalid_argument, "Need 1..16 consecutive channel commands"};
    std::vector<uint16_t> values;
    for (std::size_t i = 0; i < commands.size(); ++i) {
        if (!valid_channel(commands[i].channel) || !valid_native_command(commands[i].value) ||
            commands[i].channel != commands[0].channel + i)
            return {Status::invalid_argument, "Channels must be valid and consecutive; invalid command"};
        values.push_back(commands[i].value);
    }
    modbus::Bytes response;
    return transact(modbus::write_registers(config_.slave_id, commands[0].channel, values), 0, response);
}
} // namespace n4d3e16
