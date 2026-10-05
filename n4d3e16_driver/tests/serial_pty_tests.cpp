#include "n4d3e16/n4d3e16_driver.hpp"
#include <cerrno>
#include <exception>
#include <iostream>
#include <poll.h>
#include <pty.h>
#include <stdexcept>
#include <termios.h>
#include <thread>
#include <unistd.h>

using namespace n4d3e16;
using modbus::Bytes;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void write_all(int fd, const Bytes& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto n = ::write(fd, data.data() + sent, data.size() - sent);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw std::runtime_error("PTY write failed");
        sent += static_cast<std::size_t>(n);
    }
}
Bytes next_request(int fd) {
    Bytes request;
    const auto deadline = Clock::now() + Milliseconds(1500);
    for (;;) {
        pollfd pfd{fd, POLLIN, 0};
        const auto rc = ::poll(&pfd, 1, 20);
        if (rc > 0 && (pfd.revents & POLLIN)) {
            uint8_t byte;
            if (::read(fd, &byte, 1) == 1) request.push_back(byte);
            std::size_t length = 8;
            if (request.size() >= 7 && request[1] == 0x10) length = 9 + request[6];
            if (request.size() == length) {
                require(modbus::crc16(request.data(), request.size()) == 0, "Request CRC invalid");
                return request;
            }
        }
        if (Clock::now() >= deadline) throw std::runtime_error("PTY emulator request timed out");
    }
}
}
int main() {
    int master = -1, slave = -1;
    std::thread emulator;
    std::exception_ptr emulator_error;
    try {
        char name[128]{};
        require(::openpty(&master, &slave, name, nullptr, nullptr) == 0, "openpty failed");
        Driver io(std::make_unique<SerialPort>(name, 9600), {1, 9600, Milliseconds(250)});
        require(static_cast<bool>(io.connect()), "Real SerialPort open failed");
        termios settings{};
        require(::tcgetattr(slave, &settings) == 0 && ::cfgetispeed(&settings) == B9600 &&
                !(settings.c_cflag & (PARENB | CSTOPB)) &&
                !(settings.c_lflag & (ICANON | ECHO)), "Serial raw 9600/8N1 not set");
        ::close(slave); slave = -1;
        int reports = 0;
        io.set_input_callback([&](const InputState&) { ++reports; });
        emulator = std::thread([&] {
            try {
                for (int transaction = 0; transaction < 7; ++transaction) {
                    const auto request = next_request(master);
                    if (transaction == 5 || transaction == 6) {
                        Bytes report{1,3,2,0,1}; modbus::append_crc(report);
                        write_all(master, report);
                    }
                    Bytes reply;
                    if (request[1] == 3) {
                        require(modbus::word(request, 2) == 1 && modbus::word(request, 4) == 16,
                                "Expected 16-register output read");
                        reply = {1,3,32};
                        for (int i = 0; i < 16; ++i) {
                            reply.push_back(0); reply.push_back(i == 1 ? 1 : 0);
                        }
                    } else reply.assign(request.begin(), request.begin() + 6);
                    modbus::append_crc(reply);
                    write_all(master, Bytes(reply.begin(), reply.begin() + 3));
                    std::this_thread::sleep_for(Milliseconds(1));
                    write_all(master, Bytes(reply.begin() + 3, reply.end()));
                }
            } catch (...) { emulator_error = std::current_exception(); }
        });
        require(static_cast<bool>(io.configure_input_reporting(1)), "PTY configure failed");
        require(static_cast<bool>(io.open(2)), "PTY open channel failed");
        uint16_t mask = 0;
        require(static_cast<bool>(io.read_outputs(mask)) && mask == 2, "PTY output read failed");
        emulator.join();
        if (emulator_error) std::rethrow_exception(emulator_error);
        require(reports == 2 && io.get_inputs().active_mask == 1, "PTY report delivery failed");
        // A real tty hangup must be a transport error, not inactive inputs.
        ::close(master); master = -1;
        require(io.process_once(Milliseconds(20)).status == Status::io_error &&
                !io.get_inputs().valid, "PTY hangup not detected");
        std::cout << "Passed Linux termios / PTY integration test\n";
    } catch (const std::exception& error) {
        if (emulator.joinable()) emulator.join();
        if (master >= 0) ::close(master);
        if (slave >= 0) ::close(slave);
        std::cerr << "FAILED: " << error.what() << '\n'; return 1;
    }
    return 0;
}
