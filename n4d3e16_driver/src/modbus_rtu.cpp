#include "n4d3e16/modbus_rtu.hpp"
#include <stdexcept>

namespace n4d3e16::modbus {
namespace {
void put_word(Bytes& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value));
}
}

uint16_t crc16(const uint8_t* data, std::size_t size) {
    uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                            : static_cast<uint16_t>(crc >> 1);
    }
    return crc;
}
void append_crc(Bytes& bytes) {
    const auto crc = crc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(crc));
    bytes.push_back(static_cast<uint8_t>(crc >> 8));
}
uint16_t word(const Bytes& bytes, std::size_t offset) {
    return static_cast<uint16_t>((bytes.at(offset) << 8) | bytes.at(offset + 1));
}
Bytes read_registers(uint8_t slave, uint16_t address, uint16_t count) {
    Bytes bytes{slave, 0x03};
    put_word(bytes, address); put_word(bytes, count); append_crc(bytes);
    return bytes;
}
Bytes write_register(uint8_t slave, uint16_t address, uint16_t value) {
    Bytes bytes{slave, 0x06};
    put_word(bytes, address); put_word(bytes, value); append_crc(bytes);
    return bytes;
}
Bytes write_registers(uint8_t slave, uint16_t address,
                      const std::vector<uint16_t>& values) {
    if (values.empty() || values.size() > 123)
        throw std::invalid_argument("Invalid register count");
    Bytes bytes{slave, 0x10};
    put_word(bytes, address); put_word(bytes, static_cast<uint16_t>(values.size()));
    bytes.push_back(static_cast<uint8_t>(2 * values.size()));
    for (auto value : values) put_word(bytes, value);
    append_crc(bytes);
    return bytes;
}
void FrameParser::feed(const uint8_t* data, std::size_t size) {
    if (buffer_.size() + size > 4096) buffer_.clear();
    buffer_.insert(buffer_.end(), data, data + size);
}
bool FrameParser::pop(Bytes& frame) {
    while (buffer_.size() >= 2) {
        std::size_t length = 0;
        const auto fn = buffer_[1];
        if (fn == 0x03) {
            if (buffer_.size() < 3) return false;
            if (buffer_[2] == 0 || buffer_[2] > 250 || buffer_[2] % 2) {
                buffer_.erase(buffer_.begin()); continue;
            }
            length = 5 + buffer_[2];
        } else if (fn == 0x06 || fn == 0x10) {
            length = 8;
        } else if (fn == 0x83 || fn == 0x86 || fn == 0x90) {
            length = 5;
        } else {
            buffer_.erase(buffer_.begin()); continue;
        }
        if (buffer_.size() < length) {
            // A corrupted byte-count can otherwise hide a complete good frame
            // behind a bogus long header indefinitely. Resync only to a fully
            // CRC-validated candidate, never just to a plausible function byte.
            bool recovered = false;
            for (std::size_t offset = 1; offset + 5 <= buffer_.size(); ++offset) {
                const auto candidate_fn = buffer_[offset + 1];
                std::size_t candidate_length = 0;
                if (candidate_fn == 0x03) {
                    const auto count = buffer_[offset + 2];
                    if (count == 0 || count > 250 || count % 2) continue;
                    candidate_length = 5 + count;
                } else if (candidate_fn == 0x06 || candidate_fn == 0x10) {
                    candidate_length = 8;
                } else if (candidate_fn == 0x83 || candidate_fn == 0x86 || candidate_fn == 0x90) {
                    candidate_length = 5;
                } else continue;
                if (offset + candidate_length > buffer_.size()) continue;
                const auto candidate_crc = crc16(buffer_.data() + offset, candidate_length - 2);
                const auto end = offset + candidate_length;
                const auto received_crc = static_cast<uint16_t>(buffer_[end - 2] |
                                                                (buffer_[end - 1] << 8));
                if (candidate_crc == received_crc) {
                    buffer_.erase(buffer_.begin(), buffer_.begin() + offset);
                    recovered = true; break;
                }
            }
            if (recovered) continue;
            return false;
        }
        const auto crc = crc16(buffer_.data(), length - 2);
        const auto received = static_cast<uint16_t>(buffer_[length - 2] |
                                                    (buffer_[length - 1] << 8));
        if (crc != received) {
            ++crc_errors_; buffer_.erase(buffer_.begin()); continue;
        }
        frame.assign(buffer_.begin(), buffer_.begin() + length);
        buffer_.erase(buffer_.begin(), buffer_.begin() + length);
        return true;
    }
    return false;
}
void FrameParser::clear() { buffer_.clear(); }
} // namespace n4d3e16::modbus
