#include "solar_net/base/buffer.h"

#include <format>
#include <iostream>

int main() {
    solar_net::Buffer buffer;
    buffer.Append("GET / HTTP/1.1\r\n");
    buffer.Append("Host: localhost\r\n\r\n");

    std::cout << "Readable bytes: " << buffer.ReadableBytes() << "\n";
    std::cout << "Content:\n" << std::string(buffer.ToStringView()) << "\n";

    // Find the first CRLF line
    const auto* line_end = buffer.FindCRLF();
    if (line_end != buffer.ReaderBegin() + buffer.ReadableBytes()) {
        std::string first_line(reinterpret_cast<const char*>(buffer.ReaderBegin()),
                               static_cast<size_t>(line_end - buffer.ReaderBegin()));
        std::cout << "First line: " << first_line << "\n";
    }

    // Prepend a 4-byte length header
    buffer.PrependInt32(static_cast<int32_t>(buffer.ReadableBytes()));
    std::cout << "After prepend, readable bytes: " << buffer.ReadableBytes() << "\n";
    std::cout << "Hex: " << buffer.ToHexString() << "\n";

    return 0;
}
