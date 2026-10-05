#pragma once

#include "io_types.hpp"
#include <cstddef>
#include <string>

namespace n4d3e16 {
// open/read/write throw on transport failure. Single owner thread.
class Transport {
public:
    virtual ~Transport() = default;
    virtual void open() = 0;
    virtual void close() noexcept = 0;
    virtual std::size_t read(uint8_t* data, std::size_t capacity,
                             Milliseconds timeout) = 0;
    virtual void write(const uint8_t* data, std::size_t size,
                       Milliseconds timeout) = 0;
};

// Linux tty; adapter must provide automatic RS485 direction, or be configured
// separately for kernel-supported RS485. termios alone does not drive DE/RE.
class SerialPort final : public Transport {
public:
    SerialPort(std::string device, unsigned baud);
    ~SerialPort() override;
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    void open() override;
    void close() noexcept override;
    std::size_t read(uint8_t*, std::size_t, Milliseconds) override;
    void write(const uint8_t*, std::size_t, Milliseconds) override;
private:
    std::string device_;
    unsigned baud_;
    int fd_ = -1;
};
} // namespace n4d3e16
