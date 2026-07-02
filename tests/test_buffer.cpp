#include "solar_net/base/buffer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <unistd.h>

namespace solar_net {
namespace {

TEST(BufferTest, DefaultConstructsEmpty) {
    Buffer buffer;
    EXPECT_EQ(buffer.ReadableBytes(), 0);
    EXPECT_GE(buffer.WritableBytes(), 0);
    EXPECT_EQ(buffer.PrependableBytes(), Buffer::kInitialPrepend);
}

TEST(BufferTest, AppendAndRetrieveString) {
    Buffer buffer;
    buffer.Append("hello");
    EXPECT_EQ(buffer.ReadableBytes(), 5);
    EXPECT_EQ(buffer.ToStringView(), "hello");

    std::string retrieved = buffer.RetrieveAllAsString();
    EXPECT_EQ(retrieved, "hello");
    EXPECT_EQ(buffer.ReadableBytes(), 0);
}

TEST(BufferTest, AutoExpansion) {
    Buffer buffer(16);
    const size_t initial_capacity = buffer.Capacity();

    std::string data(1024, 'a');
    buffer.Append(data);

    EXPECT_EQ(buffer.ReadableBytes(), 1024);
    EXPECT_GT(buffer.Capacity(), initial_capacity);
    EXPECT_EQ(buffer.ToStringView(), data);
}

TEST(BufferTest, RetrievePartial) {
    Buffer buffer;
    buffer.Append("helloworld");
    buffer.Retrieve(5);
    EXPECT_EQ(buffer.ToStringView(), "world");
}

TEST(BufferTest, AppendAndReadIntegers) {
    Buffer buffer;
    buffer.AppendInt8(0x01);
    buffer.AppendInt16(0x0203);
    buffer.AppendInt32(0x04050607);
    buffer.AppendInt64(0x08090A0B0C0D0E0F);

    EXPECT_EQ(buffer.ReadInt8(), 0x01);
    EXPECT_EQ(buffer.ReadInt16(), 0x0203);
    EXPECT_EQ(buffer.ReadInt32(), 0x04050607);
    EXPECT_EQ(buffer.ReadInt64(), 0x08090A0B0C0D0E0F);
    EXPECT_EQ(buffer.ReadableBytes(), 0);
}

TEST(BufferTest, PeekIntDoesNotAdvance) {
    Buffer buffer;
    buffer.AppendInt32(42);

    EXPECT_EQ(buffer.PeekInt32(), 42);
    EXPECT_EQ(buffer.ReadableBytes(), 4);
}

TEST(BufferTest, FindCRLF) {
    Buffer buffer;
    buffer.Append("line1\r\nline2\r\n");

    const auto* crlf = buffer.FindCRLF();
    ASSERT_NE(crlf, buffer.ReaderBegin() + buffer.ReadableBytes());
    EXPECT_EQ(crlf, buffer.ReaderBegin() + 5);

    buffer.RetrieveUntil(crlf + 2);
    EXPECT_EQ(buffer.ToStringView(), "line2\r\n");
}

TEST(BufferTest, FindLF) {
    Buffer buffer;
    buffer.Append("line1\nline2\n");

    const auto* lf = buffer.FindLF();
    ASSERT_NE(lf, buffer.ReaderBegin() + buffer.ReadableBytes());
    EXPECT_EQ(lf, buffer.ReaderBegin() + 5);
}

TEST(BufferTest, PrependInt32) {
    Buffer buffer;
    buffer.Append("body");
    buffer.PrependInt32(4);

    EXPECT_EQ(buffer.ReadInt32(), 4);
    EXPECT_EQ(buffer.RetrieveAllAsString(), "body");
}

TEST(BufferTest, ReadableSpanIsZeroCopy) {
    Buffer buffer;
    buffer.Append("abcd");

    auto span = buffer.ReadableSpan();
    ASSERT_EQ(span.size(), 4);
    EXPECT_EQ(static_cast<char>(span[0]), 'a');
}

TEST(BufferTest, RetrieveUntilDelimiter) {
    Buffer buffer;
    buffer.Append("hello\r\nworld");
    buffer.RetrieveUntil("\r\n");
    EXPECT_EQ(buffer.ToStringView(), "world");
}

TEST(BufferTest, WriteFdAndReadFd) {
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    Buffer write_buf;
    write_buf.Append("hello pipe");
    int err = 0;
    EXPECT_EQ(write_buf.WriteFd(pipefd[1], &err), 10);
    EXPECT_EQ(write_buf.ReadableBytes(), 0);

    Buffer read_buf;
    EXPECT_EQ(read_buf.ReadFd(pipefd[0], &err), 10);
    EXPECT_EQ(read_buf.ToStringView(), "hello pipe");

    close(pipefd[0]);
    close(pipefd[1]);
}

TEST(BufferTest, SwapExchangesContent) {
    Buffer a;
    Buffer b;
    a.Append("aaa");
    b.Append("bb");

    a.Swap(b);
    EXPECT_EQ(a.ToStringView(), "bb");
    EXPECT_EQ(b.ToStringView(), "aaa");
}

} // namespace
} // namespace solar_net
