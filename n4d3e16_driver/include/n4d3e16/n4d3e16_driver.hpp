#pragma once

#include "modbus_rtu.hpp"
#include "serial_port.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace n4d3e16 {
// Synchronous, single owner thread. process_once() must be called while idle.
// Application callbacks should enqueue updates for that owner thread.
class Driver {
public:
    using InputCallback = std::function<void(const InputState&)>;
    Driver(std::unique_ptr<Transport> transport, DriverConfig config = {});
    ~Driver();
    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;

    Result connect();
    void disconnect();
    bool is_connected() const { return connected_; }
    void set_input_callback(InputCallback callback);
    InputState get_inputs() const { return inputs_; }
    bool input_is_fresh(Milliseconds max_age) const;
    std::size_t crc_errors() const { return parser_.crc_errors(); }
    std::size_t callback_errors() const { return callback_errors_; }

    // 0 = polling; 1..255 = report interval in seconds. Sets bitmap reporting,
    // disables board-to-board remote IO. Does not alter output channels.
    Result configure_input_reporting(uint8_t seconds);
    Result process_once(Milliseconds wait);
    Result read_inputs(uint16_t& active_mask);
    Result read_outputs(uint16_t& active_mask);
    Result read_output(uint8_t channel, bool& active);

    Result open(uint8_t channel);
    Result close(uint8_t channel);
    Result toggle(uint8_t channel);
    Result interlock(uint8_t channel);
    Result momentary(uint8_t channel);
    Result delay(uint8_t channel, unsigned seconds);
    Result open_all();
    Result close_all();
    Result write_outputs(uint16_t bitmap);
    Result write_channels(const std::vector<ChannelCommand>& commands);

private:
    Result channel_command(uint8_t channel, uint16_t value);
    Result write_register(uint16_t address, uint16_t value);
    Result transact(const modbus::Bytes& request, uint8_t expected_read_bytes,
                    modbus::Bytes& response);
    void receive(Milliseconds wait);
    void handle_input(const modbus::Bytes& frame);
    Result transport_failure(const std::exception& error);

    std::unique_ptr<Transport> transport_;
    DriverConfig config_;
    modbus::FrameParser parser_;
    InputState inputs_;
    InputCallback on_input_;
    bool connected_ = false;
    bool configured_ = false;
    bool busy_ = false;
    std::size_t callback_errors_ = 0;
};
} // namespace n4d3e16
