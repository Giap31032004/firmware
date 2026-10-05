#pragma once

#include "io_types.hpp"
#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace n4d3e16 {
struct IoRule {
    uint16_t input_mask; // IN1..IN16; any selected active input enables outputs.
    std::vector<uint8_t> outputs;
};

// Small application mapping layer. All calls use the driver owner thread.
// Shared outputs combine rules with OR. Unmapped outputs are untouched.
class IoController {
public:
    using OutputWriter = std::function<Result(uint8_t, bool)>;
    IoController(std::vector<IoRule> rules, OutputWriter writer);
    void update_inputs(uint16_t active_mask);
    Result apply();
    void invalidate_output_confirmation();
    // Call on disconnect/reconnect: require a new input sample before writing.
    void reset();
private:
    uint16_t desired_outputs() const;
    std::vector<IoRule> rules_;
    OutputWriter writer_;
    uint16_t inputs_ = 0;
    uint16_t mapped_outputs_ = 0;
    bool input_known_ = false;
    std::array<std::optional<bool>, 16> confirmed_{};
};
} // namespace n4d3e16
