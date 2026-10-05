#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace n4d3e16::modbus {
using Bytes = std::vector<uint8_t>;
uint16_t crc16(const uint8_t* data, std::size_t size);
void append_crc(Bytes& bytes);
uint16_t word(const Bytes& bytes, std::size_t offset);
Bytes read_registers(uint8_t slave, uint16_t address, uint16_t count);
Bytes write_register(uint8_t slave, uint16_t address, uint16_t value);
Bytes write_registers(uint8_t slave, uint16_t address,
                      const std::vector<uint16_t>& values);

// Parses slave responses/reports, not master requests. Retains partial frames.
class FrameParser {
public:
    void feed(const uint8_t*, std::size_t);
    bool pop(Bytes& frame);
    void clear();
    std::size_t crc_errors() const { return crc_errors_; }
private:
    Bytes buffer_;
    std::size_t crc_errors_ = 0;
};
} // namespace n4d3e16::modbus
