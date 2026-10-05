#include "n4d3e16/io_controller.hpp"
#include <stdexcept>
#include <utility>

namespace n4d3e16 {
IoController::IoController(std::vector<IoRule> rules, OutputWriter writer)
    : rules_(std::move(rules)), writer_(std::move(writer)) {
    if (rules_.empty() || !writer_)
        throw std::invalid_argument("Configure IO rules and an output writer");
    for (const auto& rule : rules_) {
        if (!rule.input_mask || rule.outputs.empty())
            throw std::invalid_argument("Each rule needs an input mask and outputs");
        uint16_t seen = 0;
        for (auto channel : rule.outputs) {
            if (channel < 1 || channel > 16)
                throw std::invalid_argument("Output channel must be 1..16");
            const auto bit = static_cast<uint16_t>(1u << (channel - 1));
            if (seen & bit)
                throw std::invalid_argument("Duplicate output within a rule");
            seen |= bit;
        }
        mapped_outputs_ |= seen;
    }
}
void IoController::update_inputs(uint16_t active_mask) {
    inputs_ = active_mask;
    input_known_ = true;
}
uint16_t IoController::desired_outputs() const {
    uint16_t desired = 0;
    for (const auto& rule : rules_)
        if (inputs_ & rule.input_mask)
            for (auto channel : rule.outputs)
                desired |= static_cast<uint16_t>(1u << (channel - 1));
    return desired;
}
Result IoController::apply() {
    if (!input_known_)
        return {Status::not_configured, "Waiting for first input sample"};
    const auto desired = desired_outputs();
    for (unsigned i = 0; i < 16; ++i) {
        if (!(mapped_outputs_ & (1u << i))) continue;
        const bool active = (desired & (1u << i)) != 0;
        if (confirmed_[i] && *confirmed_[i] == active) continue;
        // Clear before invoking the writer, including if a custom writer throws.
        confirmed_[i].reset();
        auto result = writer_(static_cast<uint8_t>(i + 1), active);
        if (!result) return result;
        // A callback may reset the controller while waiting for ACK.
        if (!input_known_) return {};
        confirmed_[i] = active;
        // Input callbacks can change desired outputs during the transaction.
        if (desired_outputs() != desired) break;
    }
    return {};
}
void IoController::invalidate_output_confirmation() {
    for (auto& value : confirmed_) value.reset();
}
void IoController::reset() {
    input_known_ = false;
    inputs_ = 0;
    invalidate_output_confirmation();
}
} // namespace n4d3e16
