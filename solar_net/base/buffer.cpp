#include "solar_net/base/buffer.h"

#include <bit>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <sys/uio.h>
#include <unistd.h>

namespace solar_net {

namespace {

constexpr std::byte kCR{static_cast<std::byte>('\r')};
constexpr std::byte kLF{static_cast<std::byte>('\n')};

template <typename T>
[[nodiscard]] T ByteSwap(T value) noexcept {
    T result{};
    const auto* src = reinterpret_cast<const std::byte*>(&value);
    auto* dst = reinterpret_cast<std::byte*>(&result) + sizeof(T) - 1;
    for (size_t i = 0; i < sizeof(T); ++i) {
        *dst-- = src[i];
    }
    return result;
}

template <typename T>
[[nodiscard]] T ToNetwork(T value) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return value;
    }
    return ByteSwap(value);
}

template <typename T>
[[nodiscard]] T FromNetwork(T value) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return value;
    }
    return ByteSwap(value);
}

} // namespace

Buffer::Buffer() : Buffer(kInitialSize) {}

Buffer::Buffer(size_t initial_size)
    : m_data(kInitialPrepend + initial_size), m_reader_index(kInitialPrepend), m_writer_index(kInitialPrepend) {}

size_t Buffer::ReadableBytes() const noexcept {
    return m_writer_index - m_reader_index;
}

size_t Buffer::WritableBytes() const noexcept {
    return m_data.size() - m_writer_index;
}

size_t Buffer::PrependableBytes() const noexcept {
    return m_reader_index;
}

size_t Buffer::Capacity() const noexcept {
    return m_data.size();
}

void Buffer::EnsureWritableBytes(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace(len);
    }
}

void Buffer::Shrink(size_t /*reserve*/) {
    // Phase 1: no-op. Future implementation can move readable bytes to a smaller buffer.
}

void Buffer::Reserve(size_t len) {
    EnsureWritableBytes(len);
}

void Buffer::Swap(Buffer& rhs) noexcept {
    m_data.swap(rhs.m_data);
    std::swap(m_reader_index, rhs.m_reader_index);
    std::swap(m_writer_index, rhs.m_writer_index);
}

const std::byte* Buffer::ReaderBegin() const noexcept {
    return Begin() + m_reader_index;
}

std::span<const std::byte> Buffer::ReadableSpan() const noexcept {
    return {ReaderBegin(), ReadableBytes()};
}

std::string_view Buffer::ToStringView() const noexcept {
    return {reinterpret_cast<const char*>(ReaderBegin()), ReadableBytes()};
}

void Buffer::Append(std::span<const std::byte> data) {
    EnsureWritableBytes(data.size());
    std::memcpy(WriterBegin(), data.data(), data.size());
    m_writer_index += data.size();
}

void Buffer::Append(std::string_view data) {
    Append(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(data.data()), data.size()});
}

void Buffer::Append(const void* data, size_t len) {
    Append(std::span<const std::byte>{static_cast<const std::byte*>(data), len});
}

void Buffer::AppendInt8(int8_t value) {
    Append(std::span<const std::byte>{reinterpret_cast<const std::byte*>(&value), sizeof(value)});
}

void Buffer::AppendInt16(int16_t value) {
    const auto network = ToNetwork(value);
    Append(std::span<const std::byte>{reinterpret_cast<const std::byte*>(&network), sizeof(network)});
}

void Buffer::AppendInt32(int32_t value) {
    const auto network = ToNetwork(value);
    Append(std::span<const std::byte>{reinterpret_cast<const std::byte*>(&network), sizeof(network)});
}

void Buffer::AppendInt64(int64_t value) {
    const auto network = ToNetwork(value);
    Append(std::span<const std::byte>{reinterpret_cast<const std::byte*>(&network), sizeof(network)});
}

void Buffer::Retrieve(size_t len) {
    if (len < ReadableBytes()) {
        m_reader_index += len;
    } else {
        RetrieveAll();
    }
}

void Buffer::RetrieveAll() {
    m_reader_index = kInitialPrepend;
    m_writer_index = kInitialPrepend;
}

void Buffer::RetrieveUntil(const std::byte* end) {
    Retrieve(static_cast<size_t>(end - ReaderBegin()));
}

void Buffer::RetrieveUntil(std::string_view delimiter) {
    const auto pos = ToStringView().find(delimiter);
    if (pos != std::string_view::npos) {
        Retrieve(pos + delimiter.size());
    }
}

std::string Buffer::RetrieveAsString(size_t len) {
    std::string result(ToStringView().substr(0, len));
    Retrieve(len);
    return result;
}

std::string Buffer::RetrieveAllAsString() {
    std::string result(ToStringView());
    RetrieveAll();
    return result;
}

const std::byte* Buffer::Peek() const noexcept {
    return ReaderBegin();
}

std::byte Buffer::PeekByte() const {
    if (ReadableBytes() < sizeof(std::byte)) {
        return std::byte{};
    }
    return *ReaderBegin();
}

int8_t Buffer::PeekInt8() const {
    if (ReadableBytes() < sizeof(int8_t)) {
        return 0;
    }
    int8_t value = 0;
    std::memcpy(&value, ReaderBegin(), sizeof(value));
    return value;
}

int16_t Buffer::PeekInt16() const {
    if (ReadableBytes() < sizeof(int16_t)) {
        return 0;
    }
    int16_t value = 0;
    std::memcpy(&value, ReaderBegin(), sizeof(value));
    return FromNetwork(value);
}

int32_t Buffer::PeekInt32() const {
    if (ReadableBytes() < sizeof(int32_t)) {
        return 0;
    }
    int32_t value = 0;
    std::memcpy(&value, ReaderBegin(), sizeof(value));
    return FromNetwork(value);
}

int64_t Buffer::PeekInt64() const {
    if (ReadableBytes() < sizeof(int64_t)) {
        return 0;
    }
    int64_t value = 0;
    std::memcpy(&value, ReaderBegin(), sizeof(value));
    return FromNetwork(value);
}

int8_t Buffer::ReadInt8() {
    const auto value = PeekInt8();
    Retrieve(sizeof(value));
    return value;
}

int16_t Buffer::ReadInt16() {
    const auto value = PeekInt16();
    Retrieve(sizeof(value));
    return value;
}

int32_t Buffer::ReadInt32() {
    const auto value = PeekInt32();
    Retrieve(sizeof(value));
    return value;
}

int64_t Buffer::ReadInt64() {
    const auto value = PeekInt64();
    Retrieve(sizeof(value));
    return value;
}

std::string Buffer::ReadString(size_t len) {
    return RetrieveAsString(len);
}

const std::byte* Buffer::FindCRLF() const {
    return FindCRLF(ReaderBegin(), ReaderBegin() + ReadableBytes());
}

const std::byte* Buffer::FindLF() const {
    return FindLF(ReaderBegin(), ReaderBegin() + ReadableBytes());
}

const std::byte* Buffer::FindEOL() const {
    return FindLF();
}

const std::byte* Buffer::FindCRLF(const std::byte* start, const std::byte* end) {
    const auto* pos = std::find(start, end, kLF);
    if (pos == end) {
        return end;
    }
    if (pos > start && *(pos - 1) == kCR) {
        return pos - 1;
    }
    return end;
}

const std::byte* Buffer::FindLF(const std::byte* start, const std::byte* end) {
    const auto* pos = std::find(start, end, kLF);
    return pos;
}

ssize_t Buffer::ReadFd(int fd, int* saved_errno) {
    char extrabuf[65536];
    const size_t writable = WritableBytes();

    struct iovec vec[2];
    vec[0].iov_base = WriterBegin();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        m_writer_index += static_cast<size_t>(n);
    } else {
        m_writer_index = m_data.size();
        Append(extrabuf, static_cast<size_t>(n) - writable);
    }

    return n;
}

ssize_t Buffer::WriteFd(int fd, int* saved_errno) {
    const ssize_t n = ::write(fd, ReaderBegin(), ReadableBytes());
    if (n < 0) {
        *saved_errno = errno;
    } else {
        Retrieve(static_cast<size_t>(n));
    }
    return n;
}

void Buffer::Prepend(std::span<const std::byte> data) {
    if (data.size() > PrependableBytes()) {
        // Shift readable bytes to the standard prepend region so we have
        // kInitialPrepend bytes of prepend space available.
        const size_t readable = ReadableBytes();
        std::memmove(Begin() + kInitialPrepend, ReaderBegin(), readable);
        m_reader_index = kInitialPrepend;
        m_writer_index = kInitialPrepend + readable;
    }
    if (data.size() > PrependableBytes()) {
        // Prepend larger than kInitialPrepend is not supported by this design.
        return;
    }
    m_reader_index -= data.size();
    std::memcpy(Begin() + m_reader_index, data.data(), data.size());
}

void Buffer::PrependInt32(int32_t value) {
    const auto network = ToNetwork(value);
    Prepend(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(&network), sizeof(network)});
}

std::string Buffer::ToHexString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto b : ReadableSpan()) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

void Buffer::MakeSpace(size_t len) {
    if (WritableBytes() + PrependableBytes() < len + kInitialPrepend) {
        m_data.resize(m_writer_index + len);
    } else {
        const size_t readable = ReadableBytes();
        std::memmove(Begin() + kInitialPrepend, ReaderBegin(), readable);
        m_reader_index = kInitialPrepend;
        m_writer_index = kInitialPrepend + readable;
    }
}

std::byte* Buffer::Begin() noexcept {
    return m_data.data();
}

const std::byte* Buffer::Begin() const noexcept {
    return m_data.data();
}

std::byte* Buffer::WriterBegin() noexcept {
    return Begin() + m_writer_index;
}

const std::byte* Buffer::WriterBegin() const noexcept {
    return Begin() + m_writer_index;
}

} // namespace solar_net
