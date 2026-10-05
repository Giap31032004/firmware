#include "n4d3e16/serial_port.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <system_error>
#include <termios.h>
#include <unistd.h>

namespace n4d3e16 {
namespace {
[[noreturn]] void system_failure(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
speed_t baud_constant(unsigned baud) {
    switch (baud) {
        case 1200: return B1200; case 2400: return B2400;
        case 4800: return B4800; case 9600: return B9600;
        case 19200: return B19200;
        default: throw std::invalid_argument("Module baud must be 1200..19200");
    }
}
int remaining_ms(Clock::time_point deadline) {
    const auto left = deadline - Clock::now();
    if (left <= Clock::duration::zero()) return 0;
    const auto ms = std::chrono::duration_cast<Milliseconds>(left).count() + 1;
    return static_cast<int>(std::min<long long>(ms, INT_MAX));
}
bool wait_fd(int fd, short events, Clock::time_point deadline) {
    for (;;) {
        pollfd pfd{fd, events, 0};
        const auto rc = ::poll(&pfd, 1, remaining_ms(deadline));
        if (rc < 0) {
            if (errno == EINTR) continue;
            system_failure("poll");
        }
        if (rc == 0) return false;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            throw std::runtime_error("Serial disconnected or unavailable");
        if (pfd.revents & events) return true;
    }
}
}
SerialPort::SerialPort(std::string device, unsigned baud)
    : device_(std::move(device)), baud_(baud) {}
SerialPort::~SerialPort() { close(); }
void SerialPort::open() {
    if (fd_ >= 0) throw std::runtime_error("Serial already open");
    const auto speed = baud_constant(baud_);
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) system_failure("open serial");
    try {
        termios settings{};
        if (::tcgetattr(fd_, &settings) < 0) system_failure("tcgetattr");
        ::cfmakeraw(&settings);
        settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
        settings.c_cflag |= CS8 | CLOCAL | CREAD;
        settings.c_cc[VMIN] = 0; settings.c_cc[VTIME] = 0;
        if (::cfsetispeed(&settings, speed) < 0 ||
            ::cfsetospeed(&settings, speed) < 0) system_failure("set baud");
        if (::tcsetattr(fd_, TCSANOW, &settings) < 0) system_failure("tcsetattr");
    } catch (...) { close(); throw; }
}
void SerialPort::close() noexcept {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}
std::size_t SerialPort::read(uint8_t* data, std::size_t capacity,
                              Milliseconds timeout) {
    if (fd_ < 0) throw std::runtime_error("Serial is closed");
    const auto deadline = Clock::now() + timeout;
    for (;;) {
        if (!wait_fd(fd_, POLLIN, deadline)) return 0;
        const auto rc = ::read(fd_, data, capacity);
        if (rc >= 0) return static_cast<std::size_t>(rc);
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            system_failure("read serial");
        if (Clock::now() >= deadline) return 0;
    }
}
void SerialPort::write(const uint8_t* data, std::size_t size,
                       Milliseconds timeout) {
    if (fd_ < 0) throw std::runtime_error("Serial is closed");
    const auto deadline = Clock::now() + timeout;
    std::size_t sent = 0;
    while (sent < size) {
        if (!wait_fd(fd_, POLLOUT, deadline))
            throw std::runtime_error("Serial write timed out; bytes may have been sent");
        const auto rc = ::write(fd_, data + sent, size - sent);
        if (rc > 0) sent += static_cast<std::size_t>(rc);
        else if (rc < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            system_failure("write serial");
        if (sent < size && Clock::now() >= deadline)
            throw std::runtime_error("Serial partial write timed out");
    }
}
} // namespace n4d3e16
