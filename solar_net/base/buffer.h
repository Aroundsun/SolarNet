#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace solar_net {

/**
 * @brief 面向网络 I/O 的可扩容连续字节缓冲区。
 *
 * 内存布局：[prepend 预留区 | 可读数据 | 可写空间]。
 * 支持 span/string_view 零拷贝读取、自动扩容，以及 readv/writev 分散读写。
 * 线程安全：否，需由调用方保证同一实例不被并发访问。
 */
class Buffer {
public:
    static constexpr size_t kInitialPrepend = 8; ///< 默认 prepend 预留字节数
    static constexpr size_t kInitialSize = 1024; ///< 默认可写区初始大小

    /** @brief 以默认容量构造空缓冲区。线程安全：否。 */
    Buffer();
    /** @brief 以指定可写区大小构造空缓冲区。线程安全：否。 */
    explicit Buffer(size_t initial_size);

    /** @brief 返回可读字节数。线程安全：否。 */
    [[nodiscard]] size_t ReadableBytes() const noexcept;
    /** @brief 返回当前可写空间（无需扩容即可写入的字节数）。线程安全：否。 */
    [[nodiscard]] size_t WritableBytes() const noexcept;
    /** @brief 返回 reader 之前可用于 Prepend 的字节数。线程安全：否。 */
    [[nodiscard]] size_t PrependableBytes() const noexcept;
    /** @brief 返回底层 vector 总容量。线程安全：否。 */
    [[nodiscard]] size_t Capacity() const noexcept;

    /** @brief 保证至少有 len 字节可写空间，不足时自动扩容或搬移数据。线程安全：否。 */
    void EnsureWritableBytes(size_t len);
    /** @brief 收缩内存占用（Phase 1 为空实现）。线程安全：否。 */
    void Shrink(size_t reserve = 0);
    /** @brief 预留可写空间，等价于 EnsureWritableBytes。线程安全：否。 */
    void Reserve(size_t len);
    /** @brief 与另一缓冲区交换内容。线程安全：否。 */
    void Swap(Buffer& rhs) noexcept;

    /** @brief 返回可读区域起始指针。线程安全：否。 */
    [[nodiscard]] const std::byte* ReaderBegin() const noexcept;
    /** @brief 返回可读数据的零拷贝 span 视图。线程安全：否。 */
    [[nodiscard]] std::span<const std::byte> ReadableSpan() const noexcept;
    /** @brief 将可读数据视为 string_view（不拷贝）。线程安全：否。 */
    [[nodiscard]] std::string_view ToStringView() const noexcept;

    /** @brief 追加字节数据。线程安全：否。 */
    void Append(std::span<const std::byte> data);
    /** @brief 追加字符串数据。线程安全：否。 */
    void Append(std::string_view data);
    /** @brief 追加原始内存块。线程安全：否。 */
    void Append(const void* data, size_t len);

    /** @brief 追加 1 字节整数（主机序）。线程安全：否。 */
    void AppendInt8(int8_t value);
    /** @brief 追加 2 字节整数（网络字节序）。线程安全：否。 */
    void AppendInt16(int16_t value);
    /** @brief 追加 4 字节整数（网络字节序）。线程安全：否。 */
    void AppendInt32(int32_t value);
    /** @brief 追加 8 字节整数（网络字节序）。线程安全：否。 */
    void AppendInt64(int64_t value);

    /** @brief 丢弃前 len 个可读字节。线程安全：否。 */
    void Retrieve(size_t len);
    /** @brief 丢弃全部可读数据并重置读写位置。线程安全：否。 */
    void RetrieveAll();
    /** @brief 丢弃到 end 指针（不含 end）之前的可读数据。线程安全：否。 */
    void RetrieveUntil(const std::byte* end);
    /** @brief 丢弃到首个 delimiter 及其本身。未找到时不做任何事。线程安全：否。 */
    void RetrieveUntil(std::string_view delimiter);
    /** @brief 取出前 len 字节为 string 并丢弃。线程安全：否。 */
    [[nodiscard]] std::string RetrieveAsString(size_t len);
    /** @brief 取出全部可读数据为 string 并清空。线程安全：否。 */
    [[nodiscard]] std::string RetrieveAllAsString();

    /** @brief 返回可读起始指针，不移动读指针。线程安全：否。 */
    [[nodiscard]] const std::byte* Peek() const noexcept;
    /** @brief 窥视首字节，不足时返回 0。线程安全：否。 */
    [[nodiscard]] std::byte PeekByte() const;
    /** @brief 窥视 int8，不足时返回 0。线程安全：否。 */
    [[nodiscard]] int8_t PeekInt8() const;
    /** @brief 窥视 int16（网络序转主机序），不足时返回 0。线程安全：否。 */
    [[nodiscard]] int16_t PeekInt16() const;
    /** @brief 窥视 int32（网络序转主机序），不足时返回 0。线程安全：否。 */
    [[nodiscard]] int32_t PeekInt32() const;
    /** @brief 窥视 int64（网络序转主机序），不足时返回 0。线程安全：否。 */
    [[nodiscard]] int64_t PeekInt64() const;

    /** @brief 读取 int8 并前移读指针。线程安全：否。 */
    [[nodiscard]] int8_t ReadInt8();
    /** @brief 读取 int16 并前移读指针。线程安全：否。 */
    [[nodiscard]] int16_t ReadInt16();
    /** @brief 读取 int32 并前移读指针。线程安全：否。 */
    [[nodiscard]] int32_t ReadInt32();
    /** @brief 读取 int64 并前移读指针。线程安全：否。 */
    [[nodiscard]] int64_t ReadInt64();
    /** @brief 读取 len 字节字符串并前移读指针。线程安全：否。 */
    [[nodiscard]] std::string ReadString(size_t len);

    /** @brief 在可读区查找 \\r\\n，找到返回 \\r 位置，否则返回可读区末尾。线程安全：否。 */
    [[nodiscard]] const std::byte* FindCRLF() const;
    /** @brief 在可读区查找 \\n，找到返回其位置，否则返回可读区末尾。线程安全：否。 */
    [[nodiscard]] const std::byte* FindLF() const;
    /** @brief 查找行结束符，当前等价于 FindLF。线程安全：否。 */
    [[nodiscard]] const std::byte* FindEOL() const;

    /** @brief 在 [start, end) 区间查找 \\r\\n。线程安全：是（静态，无共享状态）。 */
    [[nodiscard]] static const std::byte* FindCRLF(const std::byte* start, const std::byte* end);
    /** @brief 在 [start, end) 区间查找 \\n。线程安全：是（静态，无共享状态）。 */
    [[nodiscard]] static const std::byte* FindLF(const std::byte* start, const std::byte* end);

    /**
     * @brief 从 fd 读取数据到可写区（readv 分散读，超出时追加到 extrabuf）。
     * @param saved_errno 失败时写入 errno，不可为 nullptr。
     * 线程安全：否。
     */
    ssize_t ReadFd(int fd, int* saved_errno);
    /**
     * @brief 将可读数据写入 fd（write 系统调用）。
     * @param saved_errno 失败时写入 errno，不可为 nullptr。
     * 线程安全：否。
     */
    ssize_t WriteFd(int fd, int* saved_errno);

    /** @brief 在可读数据前 prepend 字节（空间不足时先搬移数据）。线程安全：否。 */
    void Prepend(std::span<const std::byte> data);
    /** @brief prepend 4 字节 int32（网络字节序）。线程安全：否。 */
    void PrependInt32(int32_t value);

    /** @brief 将可读数据格式化为十六进制字符串。线程安全：否。 */
    [[nodiscard]] std::string ToHexString() const;

private:
    void MakeSpace(size_t len);

    [[nodiscard]] std::byte* Begin() noexcept;
    [[nodiscard]] const std::byte* Begin() const noexcept;
    [[nodiscard]] std::byte* WriterBegin() noexcept;
    [[nodiscard]] const std::byte* WriterBegin() const noexcept;

    std::vector<std::byte> m_data;
    size_t m_reader_index;
    size_t m_writer_index;
};

/** @brief Buffer 可读区的只读视图别名。线程安全：取决于底层 Buffer。 */
using BufferView = std::span<const std::byte>;

} // namespace solar_net
