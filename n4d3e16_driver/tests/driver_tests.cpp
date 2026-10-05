#include "n4d3e16/n4d3e16_driver.hpp"
#include "n4d3e16/io_controller.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace n4d3e16;
using modbus::Bytes;
namespace {
int checks = 0;
void require(bool condition, const char* text) {
    ++checks;
    if (!condition) throw std::runtime_error(text);
}
Bytes read_reply(uint16_t value, uint8_t slave = 1) {
    Bytes frame{slave, 3, 2, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    modbus::append_crc(frame); return frame;
}
class FakeTransport : public Transport {
public:
    bool opened = false;
    bool no_reply = false;
    bool inject_report = false;
    bool wrong_echo = false;
    bool throw_write = false;
    uint8_t exception = 0;
    std::size_t fragment = 3;
    std::deque<uint8_t> incoming;
    std::vector<Bytes> sent;
    void open() override { opened = true; }
    void close() noexcept override { opened = false; }
    void enqueue(const Bytes& bytes) { incoming.insert(incoming.end(), bytes.begin(), bytes.end()); }
    std::size_t read(uint8_t* data, std::size_t capacity, Milliseconds timeout) override {
        if (incoming.empty()) {
            std::this_thread::sleep_for(timeout); return 0;
        }
        const auto count = std::min({capacity, fragment, incoming.size()});
        for (std::size_t i = 0; i < count; ++i) { data[i] = incoming.front(); incoming.pop_front(); }
        return count;
    }
    void write(const uint8_t* data, std::size_t size, Milliseconds) override {
        sent.emplace_back(data, data + size);
        if (throw_write) throw std::runtime_error("Injected partial write failure");
        if (no_reply) return;
        const auto& request = sent.back();
        if (inject_report) { enqueue(read_reply(0x4001)); enqueue(read_reply(0x0002, 2)); }
        if (exception) {
            Bytes frame{request[0], static_cast<uint8_t>(request[1] | 0x80), exception};
            modbus::append_crc(frame); enqueue(frame); return;
        }
        if (request[1] == 3) {
            if (modbus::word(request, 2) == 0x00C0) enqueue(read_reply(0x0004));
            else {
                Bytes frame{1, 3, 32};
                for (unsigned i = 0; i < 16; ++i) {
                    frame.push_back(0); frame.push_back(i == 0 || i == 15 ? 1 : 0);
                }
                modbus::append_crc(frame); enqueue(frame);
            }
        } else {
            Bytes frame(request.begin(), request.begin() + 6);
            if (wrong_echo) frame[5] ^= 1;
            modbus::append_crc(frame); enqueue(frame);
        }
    }
};
void test_protocol() {
    const Bytes open1{0x01,0x06,0x00,0x01,0x01,0x00,0xD9,0x9A};
    require(modbus::write_register(1, 1, 0x0100) == open1, "PDF open CH1 vector");
    require(modbus::write_register(1, 1, 0x0200) == Bytes({1,6,0,1,2,0,0xD9,0x6A}), "PDF close CH1 vector");
    require(modbus::write_register(1, 1, 0x060A) == Bytes({1,6,0,1,6,10,0x5B,0xAD}), "PDF delay vector");
    require(modbus::read_registers(1, 0xC0, 1) == Bytes({1,3,0,0xC0,0,1,0x84,0x36}), "PDF input vector");
    require(modbus::write_register(1, 0, 0x0700) == Bytes({1,6,0,0,7,0,0x8B,0xFA}), "PDF open all vector");
    require(modbus::write_register(1, 0, 0x0800) == Bytes({1,6,0,0,8,0,0x8E,0x0A}), "PDF close all vector");
    require(modbus::write_register(1, 0x70, 7) == Bytes({1,6,0,0x70,0,7,0xC9,0xD3}), "PDF bitmap vector");
    require(modbus::write_registers(1, 1, {0x0200,0x0200,0x0200,0x0200}) ==
            Bytes({1,0x10,0,1,0,4,8,2,0,2,0,2,0,2,0,0xCB,0x5A}), "PDF multi-write vector");

    modbus::FrameParser parser;
    auto report = read_reply(0x0080);
    Bytes frame;
    parser.feed(report.data(), 3);
    require(!parser.pop(frame), "Partial frame must be retained");
    parser.feed(report.data() + 3, report.size() - 3);
    parser.feed(open1.data(), open1.size());
    require(parser.pop(frame) && frame == report, "Report assembled");
    require(parser.pop(frame) && frame == open1, "Concatenated ACK assembled");
    auto bad = report; bad.back() ^= 0x55;
    parser.feed(bad.data(), bad.size()); parser.feed(report.data(), report.size());
    require(parser.pop(frame) && frame == report, "CRC failure resynchronization");
    require(parser.crc_errors() > 0, "CRC errors counted");
    const Bytes wrong_length{1,3,250,0xFF};
    parser.feed(wrong_length.data(), wrong_length.size());
    parser.feed(report.data(), report.size());
    require(parser.pop(frame) && frame == report, "Corrupted byte count cannot hide a valid later report");
}
void test_driver() {
    auto fake = std::make_unique<FakeTransport>();
    auto* bus = fake.get();
    Driver io(std::move(fake), {1, 9600, Milliseconds(30)});
    require(static_cast<bool>(io.connect()), "Connect");
    require(static_cast<bool>(io.configure_input_reporting(1)), "Configure reporting");
    require(bus->sent.size() == 5 && modbus::word(bus->sent[0], 2) == 0xFD &&
            modbus::word(bus->sent[0], 4) == 0 && modbus::word(bus->sent[3], 2) == 0xFC &&
            modbus::word(bus->sent[3], 4) == 1, "Reporting configuration order");
    int callbacks = 0;
    io.set_input_callback([&](const InputState& state) {
        ++callbacks;
        require(state.valid, "Callback gets valid input");
        require(io.open(8).status == Status::busy, "Reentrant write rejected");
    });
    bus->enqueue(read_reply(1));
    while (!bus->incoming.empty()) require(static_cast<bool>(io.process_once(Milliseconds(0))), "Receive report");
    require(io.get_inputs().first_sample && io.get_inputs().changed_mask == 0, "First sample is not an edge");
    require(io.get_inputs().active_mask == 1 && callbacks == 1, "NPN active is protocol bit 1");
    bus->enqueue(read_reply(2));
    while (!bus->incoming.empty()) io.process_once(Milliseconds(0));
    require(io.get_inputs().changed_mask == 3 && !io.get_inputs().first_sample, "Changed mask");
    bus->inject_report = true;
    require(static_cast<bool>(io.open(1)), "Unsolicited report during output ACK");
    require(io.get_inputs().active_mask == 0x4001, "Report delivered; wrong slave ignored");
    uint16_t outputs = 0;
    require(static_cast<bool>(io.read_outputs(outputs)) && outputs == 0x8001, "32-byte outputs cannot be confused with input bitmap");
    uint16_t inputs = 0;
    bus->inject_report = false;
    require(static_cast<bool>(io.read_inputs(inputs)) && inputs == 4, "Input query");
    require(static_cast<bool>(io.delay(16, 255)) && modbus::word(bus->sent.back(), 4) == 0x06FF, "Delay bounds encoding");
    require(static_cast<bool>(io.write_channels({{3,0x0100},{4,0x0200}})) && bus->sent.back()[1] == 0x10, "Multiple channel write");
    const auto before = bus->sent.size();
    require(io.open(0).status == Status::invalid_argument && io.open(17).status == Status::invalid_argument, "Invalid channel");
    require(io.delay(1, 0).status == Status::invalid_argument && io.delay(1, 256).status == Status::invalid_argument, "Invalid delay");
    require(io.write_channels({{1,0x0100},{3,0x0200}}).status == Status::invalid_argument, "Nonconsecutive channels rejected");
    require(bus->sent.size() == before, "Invalid arguments never sent");
    bus->exception = 2;
    const auto exception = io.open(1);
    require(exception.status == Status::device_exception && exception.exception_code == 2 && !exception.execution_unknown, "Exception code retained");
    bus->exception = 0; bus->wrong_echo = true;
    auto timeout = io.close(1);
    require(timeout.status == Status::timeout && timeout.execution_unknown, "Wrong echo does not confirm command");
    bus->wrong_echo = false; bus->no_reply = true;
    const auto sent = bus->sent.size();
    timeout = io.toggle(1);
    require(timeout.status == Status::timeout && timeout.execution_unknown && bus->sent.size() == sent + 1, "No toggle retry after lost ACK");
    bus->no_reply = false; bus->throw_write = true;
    auto failure = io.open(1);
    require(failure.status == Status::io_error && failure.execution_unknown && !io.is_connected(), "Partial write error is unknown and disconnects");
    require(!io.get_inputs().valid, "Disconnect invalidates input");
    require(io.close(1).status == Status::not_connected, "Closed port rejected");
    require(io.callback_errors() == 0, "Input callback checks must not have thrown");
}
void test_mapping() {
    std::vector<std::pair<uint8_t, bool>> writes;
    bool fail = false;
    IoController controller({{0x0005, {1, 2}}, {0x8000, {2, 16}}},
        [&](uint8_t ch, bool active) -> Result {
            writes.emplace_back(ch, active);
            if (fail) return {Status::timeout, "Lost ACK", 0, true};
            return {};
        });
    require(controller.apply().status == Status::not_configured && writes.empty(), "No writes before input");
    controller.update_inputs(0); controller.apply();
    require(writes == std::vector<std::pair<uint8_t,bool>>{{1,false},{2,false},{16,false}}, "Only mapped outputs close");
    writes.clear(); controller.update_inputs(1); controller.apply();
    require(writes == std::vector<std::pair<uint8_t,bool>>{{1,true},{2,true}}, "Rule enables its outputs");
    writes.clear(); controller.update_inputs(0x8000); controller.apply();
    require(writes == std::vector<std::pair<uint8_t,bool>>{{1,false},{16,true}}, "IN16 works and shared output remains on");
    writes.clear(); controller.update_inputs(0x8002); controller.apply();
    require(writes.empty(), "Unmapped input and stable output cause no writes");
    controller.update_inputs(0); fail = true;
    require(!controller.apply() && writes.size() == 1, "Lost ACK stops apply");
    writes.clear(); fail = false; controller.apply();
    require(writes == std::vector<std::pair<uint8_t,bool>>{{2,false},{16,false}}, "Failed output is not confirmed");
    writes.clear(); controller.invalidate_output_confirmation(); controller.apply();
    require(writes.size() == 3, "Invalidation reasserts mapped outputs");
    writes.clear(); controller.reset();
    require(controller.apply().status == Status::not_configured && writes.empty(), "Reset discards old input");
    controller.update_inputs(4); controller.apply();
    require(writes.size() == 3 && writes[0].second && writes[1].second, "New sample after reset");

    IoController* current = nullptr;
    writes.clear();
    IoController changing({{1, {1, 2}}}, [&](uint8_t ch, bool active) -> Result {
        writes.emplace_back(ch, active);
        if (ch == 1 && active) current->update_inputs(0);
        return {};
    });
    current = &changing;
    changing.update_inputs(1); changing.apply();
    require(writes == std::vector<std::pair<uint8_t,bool>>{{1,true}}, "Input during ACK stops stale commands");
    changing.apply();
    require(writes == std::vector<std::pair<uint8_t,bool>>{{1,true},{1,false},{2,false}}, "Next apply uses latest sample");

    const std::vector<std::vector<IoRule>> invalid{
        {}, {{0,{1}}}, {{1,{}}}, {{1,{0}}}, {{1,{17}}}, {{1,{2,2}}}
    };
    for (const auto& rules : invalid) {
        bool rejected = false;
        try { IoController bad(rules, [](uint8_t,bool) -> Result { return {}; }); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid mapping rejected");
    }
    bool rejected = false;
    try { IoController bad({{1,{1}}}, {}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Missing writer rejected");
}

}
int main() {
    try {
        test_protocol(); test_driver(); test_mapping();
        std::cout << "Passed " << checks << " checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n'; return 1;
    }
}
