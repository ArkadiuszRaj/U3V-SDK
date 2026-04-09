#pragma once

/*
 * u3v_camera.hpp - Simple C++ class for USB3 Vision camera access
 *
 * Provides camera discovery, register read/write, GenICam feature access,
 * and image streaming via the usb3vision Linux kernel driver (/dev/u3vX).
 */

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <poll.h>

#include <zip.h>

#include "xml_parser/NodeMap.hpp"

namespace u3v {

// ── Kernel driver ioctl interface ───────────────────────────────────────────

#define U3V_MAGIC 0x5D

struct u3v_read_memory {
    __u64 address;
    void* u_buffer;
    __u32 transfer_size;
    __u32* u_bytes_read;
};

struct u3v_write_memory {
    __u64 address;
    const void* u_buffer;
    __u32 transfer_size;
    __u32* u_bytes_written;
};

struct u3v_get_stream_alignment {
    __u32* u_stream_alignment;
};

struct u3v_get_os_max_transfer {
    __u32* u_os_max_transfer_size;
};

struct u3v_configure_stream {
    __u64 image_buffer_size;
    __u64 chunk_data_buffer_size;
    __u32 max_urb_size;
    __u32* u_max_leader_size;
    __u32* u_max_trailer_size;
};

struct u3v_configure_buffer {
    void* u_image_buffer;
    void* u_chunk_data_buffer;
    __u64* u_buffer_handle;
};

struct u3v_unconfigure_buffer {
    __u64 buffer_handle;
};

struct u3v_queue_buffer {
    __u64 buffer_handle;
};

struct u3v_wait_for_buffer {
    __u64 buffer_handle;
    void* u_leader_buffer;
    __u32* u_leader_size;
    void* u_trailer_buffer;
    __u32* u_trailer_size;
    void* u_buffer_complete_data;
};

#define U3V_IOCTL_READ             _IOWR(U3V_MAGIC,  1, struct u3v_read_memory)
#define U3V_IOCTL_WRITE            _IOWR(U3V_MAGIC,  2, struct u3v_write_memory)
#define U3V_IOCTL_GET_STREAM_ALIGN _IOR(U3V_MAGIC,   3, struct u3v_get_stream_alignment)
#define U3V_IOCTL_GET_OS_MAX_XFER  _IOR(U3V_MAGIC,   4, struct u3v_get_os_max_transfer)
#define U3V_IOCTL_CONFIGURE_STREAM _IOWR(U3V_MAGIC,  5, struct u3v_configure_stream)
#define U3V_IOCTL_UNCONFIGURE_STREAM _IO(U3V_MAGIC,   6)
#define U3V_IOCTL_CONFIGURE_BUFFER _IOWR(U3V_MAGIC,  7, struct u3v_configure_buffer)
#define U3V_IOCTL_UNCONFIGURE_BUF  _IOW(U3V_MAGIC,   8, struct u3v_unconfigure_buffer)
#define U3V_IOCTL_QUEUE_BUFFER     _IOW(U3V_MAGIC,   9, struct u3v_queue_buffer)
#define U3V_IOCTL_WAIT_FOR_BUFFER  _IOWR(U3V_MAGIC, 10, struct u3v_wait_for_buffer)
#define U3V_IOCTL_CANCEL_ALL_BUFS  _IO(U3V_MAGIC,   11)

// ── ABRM register offsets ───────────────────────────────────────────────────

constexpr uint64_t ABRM_GENCP_VERSION           = 0x00000;
constexpr uint64_t ABRM_MANUFACTURER_NAME       = 0x00004;
constexpr uint64_t ABRM_MODEL_NAME              = 0x00044;
constexpr uint64_t ABRM_FAMILY_NAME             = 0x00084;
constexpr uint64_t ABRM_DEVICE_VERSION          = 0x000C4;
constexpr uint64_t ABRM_MANUFACTURER_INFO       = 0x00104;
constexpr uint64_t ABRM_SERIAL_NUMBER           = 0x00144;
constexpr uint64_t ABRM_USER_DEFINED_NAME       = 0x00184;
constexpr uint64_t ABRM_DEVICE_CAPABILITY       = 0x001C4;
constexpr uint64_t ABRM_MANIFEST_TABLE_ADDRESS  = 0x001D0;
constexpr uint64_t ABRM_SBRM_ADDRESS            = 0x001D8;
constexpr uint64_t ABRM_DEVICE_CONFIGURATION    = 0x001E0;
constexpr uint64_t ABRM_HEARTBEAT_TIMEOUT       = 0x001E8;
constexpr uint64_t ABRM_TIMESTAMP               = 0x001F0;
constexpr uint64_t ABRM_TIMESTAMP_LATCH         = 0x001F8;
constexpr uint64_t ABRM_ACCESS_PRIVILEGE        = 0x00204;

// SBRM register offsets (relative to SBRM base)
constexpr uint64_t SBRM_U3V_VERSION             = 0x00000;
constexpr uint64_t SBRM_U3VCP_CAPABILITY        = 0x00004;
constexpr uint64_t SBRM_MAX_CMD_TRANSFER        = 0x00014;
constexpr uint64_t SBRM_MAX_ACK_TRANSFER        = 0x00018;
constexpr uint64_t SBRM_NUM_STREAM_CHANNELS     = 0x0001C;
constexpr uint64_t SBRM_SIRM_ADDRESS            = 0x00020;
constexpr uint64_t SBRM_SIRM_LENGTH             = 0x00028;
constexpr uint64_t SBRM_EIRM_ADDRESS            = 0x0002C;
constexpr uint64_t SBRM_EIRM_LENGTH             = 0x00034;
constexpr uint64_t SBRM_CURRENT_SPEED            = 0x00040;

// U3V protocol constants
constexpr uint32_t U3V_LEADER_MAGIC  = 0x4C563355; // "U3VL"
constexpr uint32_t U3V_TRAILER_MAGIC = 0x54563355; // "U3VT"
constexpr uint16_t U3V_PAYLOAD_IMAGE = 0x0001;
constexpr uint16_t U3V_PAYLOAD_CHUNK = 0x4000;

// ABRM string register length
constexpr uint32_t ABRM_STRING_LEN = 64;
// GenCP safe read chunk size
constexpr uint32_t GENCP_READ_CHUNK = 512;

// ── Leader / Trailer structures ─────────────────────────────────────────────

struct __attribute__((packed)) LeaderHeader {
    uint32_t magic;
    uint16_t reserved0;
    uint16_t leader_size;
    uint64_t block_id;
    uint16_t reserved1;
    uint16_t payload_type;
};

struct __attribute__((packed)) TrailerHeader {
    uint32_t magic;
    uint16_t reserved0;
    uint16_t trailer_size;
    uint64_t block_id;
    uint16_t status;
    uint16_t reserved1;
    uint64_t valid_payload_size;
};

// ── Frame data returned by get_frame() ──────────────────────────────────────

struct Frame {
    std::vector<uint8_t> image_data;
    uint64_t block_id = 0;
    uint64_t timestamp = 0;
    uint16_t payload_type = 0;
    uint16_t status = 0;
    uint64_t valid_payload_size = 0;
};

// ── Internal buffer tracking ────────────────────────────────────────────────

struct StreamBuffer {
    std::vector<uint8_t> image;
    __u64 handle = 0;
    bool queued = false;
};

// ── Camera class ────────────────────────────────────────────────────────────

class Camera {
public:
    Camera() = default;

    ~Camera() {
        if (streaming_)
            stop_streaming();
        close();
    }

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    // Not moveable (contains mutex, condition_variable, thread)
    Camera(Camera&&) = delete;
    Camera& operator=(Camera&&) = delete;

    // ── Device discovery & open ─────────────────────────────────────────

    /// List all available /dev/u3v* devices
    static std::vector<std::string> enumerate() {
        std::vector<std::string> devices;
        namespace fs = std::filesystem;
        if (!fs::exists("/dev")) return devices;

        for (const auto& entry : fs::directory_iterator("/dev")) {
            const auto& name = entry.path().filename().string();
            if (name.starts_with("u3v") && name.size() > 3) {
                devices.push_back(entry.path().string());
            }
        }
        std::sort(devices.begin(), devices.end());
        return devices;
    }

    /// Open first available camera or specific device path
    void open(const std::string& device_path = "") {
        if (fd_ >= 0)
            throw std::runtime_error("Camera already open");

        std::string path = device_path;
        if (path.empty()) {
            auto devs = enumerate();
            if (devs.empty())
                throw std::runtime_error("No U3V cameras found (is the u3v kernel module loaded?)");
            path = devs.front();
        }

        fd_ = ::open(path.c_str(), O_RDWR);
        if (fd_ < 0)
            throw std::runtime_error("Failed to open " + path + ": " + std::strerror(errno));
        dev_path_ = path;
    }

    /// Open camera matching a serial number
    void open_by_serial(const std::string& serial) {
        if (fd_ >= 0)
            throw std::runtime_error("Camera already open");

        for (const auto& dev : enumerate()) {
            int tmp_fd = ::open(dev.c_str(), O_RDWR);
            if (tmp_fd < 0) continue;

            char buf[ABRM_STRING_LEN] = {};
            if (read_mem(tmp_fd, ABRM_SERIAL_NUMBER, buf, ABRM_STRING_LEN)) {
                std::string dev_serial(buf, strnlen(buf, ABRM_STRING_LEN));
                if (dev_serial == serial) {
                    fd_ = tmp_fd;
                    dev_path_ = dev;
                    return;
                }
            }
            ::close(tmp_fd);
        }
        throw std::runtime_error("No camera found with serial: " + serial);
    }

    /// Open camera matching a model name
    void open_by_model(const std::string& model) {
        if (fd_ >= 0)
            throw std::runtime_error("Camera already open");

        for (const auto& dev : enumerate()) {
            int tmp_fd = ::open(dev.c_str(), O_RDWR);
            if (tmp_fd < 0) continue;

            char buf[ABRM_STRING_LEN] = {};
            if (read_mem(tmp_fd, ABRM_MODEL_NAME, buf, ABRM_STRING_LEN)) {
                std::string dev_model(buf, strnlen(buf, ABRM_STRING_LEN));
                if (dev_model == model) {
                    fd_ = tmp_fd;
                    dev_path_ = dev;
                    return;
                }
            }
            ::close(tmp_fd);
        }
        throw std::runtime_error("No camera found with model: " + model);
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
            dev_path_.clear();
        }
    }

    bool is_open() const { return fd_ >= 0; }
    const std::string& device_path() const { return dev_path_; }

    // ── Register access ─────────────────────────────────────────────────

    /// Read raw bytes from a device register address
    void read_reg(uint64_t address, void* buffer, uint32_t size) {
        ensure_open();
        if (!read_mem(fd_, address, buffer, size))
            throw std::runtime_error("read_reg failed at 0x" + to_hex(address));
    }

    /// Write raw bytes to a device register address
    void write_reg(uint64_t address, const void* buffer, uint32_t size) {
        ensure_open();
        if (!write_mem(fd_, address, buffer, size))
            throw std::runtime_error("write_reg failed at 0x" + to_hex(address));
    }

    /// Read a typed value from a register
    template<typename T>
    T read_reg(uint64_t address) {
        T val{};
        read_reg(address, &val, sizeof(T));
        return val;
    }

    /// Write a typed value to a register
    template<typename T>
    void write_reg(uint64_t address, T value) {
        write_reg(address, &value, sizeof(T));
    }

    // ── ABRM convenience accessors ──────────────────────────────────────

    std::string manufacturer_name() { return read_abrm_string(ABRM_MANUFACTURER_NAME); }
    std::string model_name()        { return read_abrm_string(ABRM_MODEL_NAME); }
    std::string family_name()       { return read_abrm_string(ABRM_FAMILY_NAME); }
    std::string device_version()    { return read_abrm_string(ABRM_DEVICE_VERSION); }
    std::string serial_number()     { return read_abrm_string(ABRM_SERIAL_NUMBER); }

    uint32_t gencp_version() { return read_reg<uint32_t>(ABRM_GENCP_VERSION); }
    uint64_t sbrm_address()  { return read_reg<uint64_t>(ABRM_SBRM_ADDRESS); }

    // ── GenICam XML / Feature access ────────────────────────────────────

    /// Download GenICam XML from camera, parse it, build NodeMap.
    /// Must be called before using get/set_feature().
    void load_xml() {
        ensure_open();
        std::string xml_str = download_device_xml();
        if (!node_map_.init(xml_str.c_str()))
            throw std::runtime_error("Failed to parse GenICam XML from camera");
        xml_loaded_ = true;
    }

    /// Load GenICam XML from a local file (e.g. previously saved by dump_device_xml)
    void load_xml_file(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open())
            throw std::runtime_error("Cannot open XML file: " + path);
        std::ostringstream ss;
        ss << f.rdbuf();
        if (!node_map_.init(ss.str().c_str()))
            throw std::runtime_error("Failed to parse GenICam XML: " + path);
        xml_loaded_ = true;
    }

    bool is_xml_loaded() const { return xml_loaded_; }
    const NodeMap& node_map() const { return node_map_; }

    /// List all public feature names
    std::vector<std::string> list_features() const {
        ensure_xml();
        std::vector<std::string> names;
        names.reserve(node_map_.features().size());
        for (const auto& [name, _] : node_map_.features())
            names.push_back(name);
        std::sort(names.begin(), names.end());
        return names;
    }

    /// Find features matching a regex pattern (case-insensitive by default)
    /// E.g. find_features("exposure") -> {"ExposureAuto", "ExposureTime", ...}
    std::vector<std::string> find_features(const std::string& pattern,
                                           bool case_insensitive = true) const {
        ensure_xml();
        auto flags = std::regex::ECMAScript;
        if (case_insensitive)
            flags |= std::regex::icase;

        std::regex re(pattern, flags);
        std::vector<std::string> matches;
        for (const auto& [name, _] : node_map_.features()) {
            if (std::regex_search(name, re))
                matches.push_back(name);
        }
        std::sort(matches.begin(), matches.end());
        return matches;
    }

    /// Describe a feature: name, type, address, access, limits, enum values
    struct FeatureInfo {
        std::string name;
        NodeKind kind = NodeKind::Unknown;
        std::optional<uint64_t> address;
        std::optional<uint64_t> length;
        std::optional<AccessMode> access;
        std::optional<int64_t> value;       // current value (read from device)
        std::optional<int64_t> min;
        std::optional<int64_t> max;
        std::optional<int64_t> inc;
        std::optional<std::string> unit;
        std::vector<std::pair<std::string, uint64_t>> enum_entries;
    };

    FeatureInfo describe_feature(const std::string& name) {
        ensure_xml();
        ensure_open();
        auto resolved = resolve_feature(name);
        FeatureInfo info;
        info.name = name;
        info.kind = resolved.node->kind;
        info.address = resolved.address;
        info.length = resolved.length;
        info.access = resolved.access;
        info.unit = resolved.top_node->unit;

        // Static limits from XML
        if (resolved.top_node->min) info.min = static_cast<int64_t>(*resolved.top_node->min);
        if (resolved.top_node->max) info.max = static_cast<int64_t>(*resolved.top_node->max);
        if (resolved.top_node->inc) info.inc = static_cast<int64_t>(*resolved.top_node->inc);

        // pMin / pMax: read dynamic limits from device
        if (resolved.top_node->pMin) {
            try {
                auto min_resolved = resolve_node(*node_map_.findNode(*resolved.top_node->pMin));
                info.min = read_register_value(min_resolved);
            } catch (...) {}
        }
        if (resolved.top_node->pMax) {
            try {
                auto max_resolved = resolve_node(*node_map_.findNode(*resolved.top_node->pMax));
                info.max = read_register_value(max_resolved);
            } catch (...) {}
        }

        // Try reading current value
        if (resolved.address && resolved.length && resolved.length <= 8) {
            try {
                info.value = read_register_value(resolved);
            } catch (...) {}
        }

        // Enum entries
        if (!resolved.top_node->enumEntries.empty()) {
            for (const auto& e : resolved.top_node->enumEntries)
                info.enum_entries.emplace_back(e.name, e.value);
        }

        return info;
    }

    /// Read a GenICam feature value by name (e.g. "ExposureTime", "Width", "Gain")
    /// Returns the raw integer value from the resolved register.
    int64_t get_feature(const std::string& name) {
        ensure_xml();
        ensure_open();
        auto resolved = resolve_feature(name);
        return read_register_value(resolved);
    }

    /// Write a GenICam feature value by name
    void set_feature(const std::string& name, int64_t value) {
        ensure_xml();
        ensure_open();
        auto resolved = resolve_feature(name);

        if (resolved.access && *resolved.access == AccessMode::RO)
            throw std::runtime_error("Feature '" + name + "' is read-only");

        write_register_value(resolved, value);
    }

    /// Read an Enumeration feature as a string name
    std::string get_enum_feature(const std::string& name) {
        ensure_xml();
        ensure_open();
        auto resolved = resolve_feature(name);
        int64_t val = read_register_value(resolved);

        for (const auto& e : resolved.top_node->enumEntries) {
            if (static_cast<int64_t>(e.value) == val)
                return e.name;
        }
        return std::to_string(val);
    }

    /// Write an Enumeration feature by entry name (e.g. "Continuous", "Off")
    void set_enum_feature(const std::string& feature_name, const std::string& entry_name) {
        ensure_xml();
        ensure_open();
        auto resolved = resolve_feature(feature_name);

        for (const auto& e : resolved.top_node->enumEntries) {
            if (e.name == entry_name) {
                write_register_value(resolved, static_cast<int64_t>(e.value));
                return;
            }
        }
        throw std::runtime_error("Unknown enum entry '" + entry_name
            + "' for feature '" + feature_name + "'");
    }

    /// Execute a Command feature (e.g. "AcquisitionStart", "AcquisitionStop")
    void execute_command(const std::string& name) {
        ensure_xml();
        ensure_open();
        const NodeInfo* node = node_map_.findNode(name);
        if (!node)
            throw std::runtime_error("Feature not found: " + name);

        if (node->kind != NodeKind::Command)
            throw std::runtime_error("'" + name + "' is not a Command feature");

        // Command nodes write commandValue to the pValue register
        auto resolved = resolve_node(*node);

        int64_t cmd_val = 1;
        if (node->commandValue)
            cmd_val = static_cast<int64_t>(*node->commandValue);

        write_register_value(resolved, cmd_val);
    }

    // ── Streaming ───────────────────────────────────────────────────────

    /// Configure buffers and start streaming
    ///
    /// @param image_buffer_size  Expected max image payload size in bytes
    /// @param buffer_count       Number of buffers to allocate and queue
    void start_streaming(uint64_t image_buffer_size, uint32_t buffer_count = 4) {
        ensure_open();
        if (streaming_)
            throw std::runtime_error("Already streaming");
        if (buffer_count == 0)
            throw std::invalid_argument("buffer_count must be > 0");

        // Query stream alignment and max transfer size
        uint32_t alignment = 1;
        u3v_get_stream_alignment align_req = { .u_stream_alignment = &alignment };
        ioctl(fd_, U3V_IOCTL_GET_STREAM_ALIGN, &align_req); // best-effort

        uint32_t max_transfer = 0;
        u3v_get_os_max_transfer xfer_req = { .u_os_max_transfer_size = &max_transfer };
        ioctl(fd_, U3V_IOCTL_GET_OS_MAX_XFER, &xfer_req);

        uint32_t urb_size = max_transfer > 0 ? max_transfer : (1024 * 1024);

        // Align image buffer size
        if (alignment > 1)
            image_buffer_size = ((image_buffer_size + alignment - 1) / alignment) * alignment;

        // Configure stream channel
        max_leader_size_ = 0;
        max_trailer_size_ = 0;
        u3v_configure_stream cfg = {
            .image_buffer_size = image_buffer_size,
            .chunk_data_buffer_size = 0,
            .max_urb_size = urb_size,
            .u_max_leader_size = &max_leader_size_,
            .u_max_trailer_size = &max_trailer_size_,
        };

        if (ioctl(fd_, U3V_IOCTL_CONFIGURE_STREAM, &cfg) != 0) {
            throw std::runtime_error(
                "CONFIGURE_STREAM failed: " + std::string(std::strerror(errno)));
        }

        // Allocate and configure buffers
        buffers_.resize(buffer_count);
        for (uint32_t i = 0; i < buffer_count; i++) {
            auto& buf = buffers_[i];
            buf.image.resize(image_buffer_size, 0);
            buf.handle = 0;
            buf.queued = false;

            u3v_configure_buffer cbuf = {
                .u_image_buffer = buf.image.data(),
                .u_chunk_data_buffer = nullptr,
                .u_buffer_handle = &buf.handle,
            };

            if (ioctl(fd_, U3V_IOCTL_CONFIGURE_BUFFER, &cbuf) != 0) {
                cleanup_streaming();
                throw std::runtime_error(
                    "CONFIGURE_BUFFER failed: " + std::string(std::strerror(errno)));
            }
        }

        // Queue all buffers
        for (auto& buf : buffers_) {
            u3v_queue_buffer qb = { .buffer_handle = buf.handle };
            if (ioctl(fd_, U3V_IOCTL_QUEUE_BUFFER, &qb) != 0) {
                cleanup_streaming();
                throw std::runtime_error(
                    "QUEUE_BUFFER failed: " + std::string(std::strerror(errno)));
            }
            buf.queued = true;
        }

        streaming_ = true;
        next_buf_idx_ = 0;
    }

    /// Grab a frame with timeout. Returns std::nullopt on timeout.
    std::optional<Frame> get_frame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        if (!streaming_)
            throw std::runtime_error("Not streaming");
        if (timeout.count() < 0)
            throw std::invalid_argument("timeout must be >= 0");

        auto& buf = buffers_[next_buf_idx_];
        if (!buf.queued)
            throw std::runtime_error("Buffer not queued (internal error)");

        // Prepare leader/trailer receive buffers
        std::vector<uint8_t> leader_buf(max_leader_size_, 0);
        std::vector<uint8_t> trailer_buf(max_trailer_size_, 0);
        uint32_t leader_size = max_leader_size_;
        uint32_t trailer_size = max_trailer_size_;

        u3v_wait_for_buffer wb = {
            .buffer_handle = buf.handle,
            .u_leader_buffer = leader_buf.data(),
            .u_leader_size = &leader_size,
            .u_trailer_buffer = trailer_buf.data(),
            .u_trailer_size = &trailer_size,
            .u_buffer_complete_data = nullptr,
        };

        if (timeout.count() == 0)
            return std::nullopt;

        struct pollfd pfd = {
            .fd = fd_,
            .events = POLLIN,
            .revents = 0
        };
        int poll_ret = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
        if (poll_ret == 0)
            return std::nullopt;
        if (poll_ret < 0)
            throw std::runtime_error("poll() failed before WAIT_FOR_BUFFER: "
                + std::string(std::strerror(errno)));

        int ret = ioctl(fd_, U3V_IOCTL_WAIT_FOR_BUFFER, &wb);
        buf.queued = false;

        // Re-queue the buffer immediately for continuous streaming
        u3v_queue_buffer qb = { .buffer_handle = buf.handle };
        if (ioctl(fd_, U3V_IOCTL_QUEUE_BUFFER, &qb) == 0)
            buf.queued = true;

        next_buf_idx_ = (next_buf_idx_ + 1) % buffers_.size();

        if (ret != 0)
            return std::nullopt;

        // Parse leader
        Frame frame;
        if (leader_size >= sizeof(LeaderHeader)) {
            auto* lh = reinterpret_cast<const LeaderHeader*>(leader_buf.data());
            frame.block_id = lh->block_id;
            frame.payload_type = lh->payload_type;
        }

        // Parse trailer
        if (trailer_size >= sizeof(TrailerHeader)) {
            auto* th = reinterpret_cast<const TrailerHeader*>(trailer_buf.data());
            frame.status = th->status;
            frame.valid_payload_size = th->valid_payload_size;
        }

        // Copy image data
        size_t data_size = frame.valid_payload_size > 0
            ? std::min(static_cast<size_t>(frame.valid_payload_size), buf.image.size())
            : buf.image.size();
        frame.image_data.assign(buf.image.begin(), buf.image.begin() + data_size);

        return frame;
    }

    /// Stop streaming and release all buffers
    void stop_streaming() {
        if (!streaming_)
            return;
        stop_grabber();
        cleanup_streaming();
        streaming_ = false;
    }

    bool is_streaming() const { return streaming_; }

    // ── Asynchronous frame grabber ──────────────────────────────────────

    /// Start a background thread that continuously grabs frames into a ring buffer.
    /// Frames are available via pop_frame(). Old frames are dropped when the
    /// queue is full (newest-wins policy).
    ///
    /// @param queue_size  Max frames held in the ring buffer (default 5)
    void start_grabber(uint32_t queue_size = 5) {
        if (!streaming_)
            throw std::runtime_error("Must call start_streaming() before start_grabber()");
        if (grabber_running_.load())
            throw std::runtime_error("Grabber already running");

        frame_queue_max_ = queue_size;
        frame_queue_.clear();
        grabber_running_.store(true);
        grabber_thread_ = std::thread(&Camera::grabber_loop, this);
    }

    /// Stop the background grabber thread
    void stop_grabber() {
        if (!grabber_running_.load())
            return;
        grabber_running_.store(false);
        if (streaming_)
            ioctl(fd_, U3V_IOCTL_CANCEL_ALL_BUFS);
        if (grabber_thread_.joinable())
            grabber_thread_.join();
    }

    bool is_grabber_running() const { return grabber_running_.load(); }

    /// Pop the newest frame from the queue (non-blocking).
    /// Returns std::nullopt if the queue is empty.
    std::optional<Frame> pop_frame() {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        if (frame_queue_.empty())
            return std::nullopt;
        Frame f = std::move(frame_queue_.back());
        frame_queue_.clear();  // drop older frames, keep only latest
        return f;
    }

    /// Wait for a frame with timeout. Returns std::nullopt on timeout.
    std::optional<Frame> wait_frame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(frame_mutex_);
        if (frame_cv_.wait_for(lock, timeout, [&] { return !frame_queue_.empty(); })) {
            Frame f = std::move(frame_queue_.back());
            frame_queue_.clear();
            return f;
        }
        return std::nullopt;
    }

    /// Number of frames currently in the queue
    size_t frame_queue_size() const {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        return frame_queue_.size();
    }

    /// Total frames grabbed since start_grabber() (including dropped)
    uint64_t frames_grabbed() const { return frames_grabbed_.load(); }

    /// Total frames dropped due to full queue
    uint64_t frames_dropped() const { return frames_dropped_.load(); }

private:
    int fd_ = -1;
    std::string dev_path_;
    bool streaming_ = false;
    std::vector<StreamBuffer> buffers_;
    uint32_t max_leader_size_ = 0;
    uint32_t max_trailer_size_ = 0;
    uint32_t next_buf_idx_ = 0;

    NodeMap node_map_;
    bool xml_loaded_ = false;

    // Async grabber state
    std::thread grabber_thread_;
    std::atomic<bool> grabber_running_{false};
    mutable std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    std::vector<Frame> frame_queue_;
    uint32_t frame_queue_max_ = 5;
    std::atomic<uint64_t> frames_grabbed_{0};
    std::atomic<uint64_t> frames_dropped_{0};

    void grabber_loop() {
        while (grabber_running_.load()) {
            auto frame = get_frame(std::chrono::milliseconds(100));
            if (!frame)
                continue;

            frames_grabbed_.fetch_add(1);

            std::lock_guard<std::mutex> lock(frame_mutex_);
            if (frame_queue_.size() >= frame_queue_max_) {
                frame_queue_.erase(frame_queue_.begin());
                frames_dropped_.fetch_add(1);
            }
            frame_queue_.push_back(std::move(*frame));
            frame_cv_.notify_one();
        }
    }

    void ensure_open() const {
        if (fd_ < 0)
            throw std::runtime_error("Camera not open");
    }

    void ensure_xml() const {
        if (!xml_loaded_)
            throw std::runtime_error("GenICam XML not loaded. Call load_xml() first.");
    }

    // ── Resolved register info ──────────────────────────────────────────

    struct ResolvedRegister {
        const NodeInfo* top_node = nullptr;   // the original feature node
        const NodeInfo* node = nullptr;       // the resolved register node
        std::optional<uint64_t> address;
        std::optional<uint64_t> length;
        std::optional<AccessMode> access;
        Endianess endian = Endianess::Little;
        std::optional<BitField> bitfield;
    };

    // Walk the pValue chain to find the underlying register node
    ResolvedRegister resolve_node(const NodeInfo& start, int depth = 0) const {
        if (depth > 16)
            throw std::runtime_error("pValue chain too deep for: " + start.name);

        const NodeInfo* node = &start;

        // Follow pValue indirection
        if (node->pValue) {
            const NodeInfo* target = node_map_.findNode(*node->pValue);
            if (target)
                return resolve_deeper(start, *target, depth);
        }

        // This node is the final register
        ResolvedRegister r;
        r.top_node = &start;
        r.node = node;
        r.address = node->address;
        r.length = node->length;
        r.endian = node->endianess.value_or(Endianess::Little);

        // Determine access mode: prefer the top-level imposedAccess, then node's own
        if (start.imposedAccess)
            r.access = start.imposedAccess;
        else if (node->accessMode)
            r.access = node->accessMode;

        // MaskedIntReg / StructEntry bitfield
        if (node->bitField)
            r.bitfield = node->bitField;

        // StructEntry: resolve address from parent StructReg
        if (node->kind == NodeKind::StructEntry && node->bitField) {
            const NodeInfo* parent = node_map_.findNode(node->bitField->parentStruct);
            if (parent) {
                r.address = parent->address;
                r.length = parent->length;
                r.endian = parent->endianess.value_or(Endianess::Little);
            }
        }

        return r;
    }

    ResolvedRegister resolve_deeper(const NodeInfo& top, const NodeInfo& target, int depth) const {
        auto r = resolve_node(target, depth + 1);
        r.top_node = &top;  // keep original feature as top_node
        // Propagate access from top if set
        if (top.imposedAccess)
            r.access = top.imposedAccess;
        return r;
    }

    ResolvedRegister resolve_feature(const std::string& name) const {
        const NodeInfo* node = node_map_.findNode(name);
        if (!node)
            throw std::runtime_error("Feature not found: " + name);
        return resolve_node(*node);
    }

    // Read a value from a resolved register, handling endianess and bitfields
    int64_t read_register_value(const ResolvedRegister& r) {
        if (!r.address)
            throw std::runtime_error("Feature '" + r.top_node->name + "' has no register address");

        uint64_t len = r.length.value_or(4);
        if (len > 8)
            throw std::runtime_error("Register too large (" + std::to_string(len) + " bytes) for integer read");

        uint64_t raw = 0;
        read_reg(*r.address, &raw, static_cast<uint32_t>(len));

        // Byte-swap if big endian
        if (r.endian == Endianess::Big)
            raw = swap_bytes(raw, len);

        // Apply bitfield mask
        if (r.bitfield) {
            uint8_t lsb = r.bitfield->lsb;
            uint8_t msb = r.bitfield->msb;
            uint64_t mask = make_bit_mask(lsb, msb);
            raw = (raw & mask) >> lsb;
        }

        // Sign extend if signed
        if (r.node->sign && *r.node->sign == Sign::Signed) {
            uint8_t bits = r.bitfield
                ? (r.bitfield->msb - r.bitfield->lsb + 1)
                : static_cast<uint8_t>(len * 8);
            if (bits < 64 && (raw >> (bits - 1)) & 1)
                raw |= ~((1ULL << bits) - 1);
        }

        return static_cast<int64_t>(raw);
    }

    // Write a value to a resolved register, handling endianess and bitfields
    void write_register_value(const ResolvedRegister& r, int64_t value) {
        if (!r.address)
            throw std::runtime_error("Feature '" + r.top_node->name + "' has no register address");

        uint64_t len = r.length.value_or(4);
        if (len > 8)
            throw std::runtime_error("Register too large for integer write");

        uint64_t raw = static_cast<uint64_t>(value);

        if (r.bitfield) {
            // Read-modify-write for bitfield registers
            uint64_t current = 0;
            read_reg(*r.address, &current, static_cast<uint32_t>(len));
            if (r.endian == Endianess::Big)
                current = swap_bytes(current, len);

            uint8_t lsb = r.bitfield->lsb;
            uint8_t msb = r.bitfield->msb;
            uint64_t mask = make_bit_mask(lsb, msb);
            current = (current & ~mask) | ((raw << lsb) & mask);
            raw = current;
        }

        if (r.endian == Endianess::Big)
            raw = swap_bytes(raw, len);

        write_reg(*r.address, &raw, static_cast<uint32_t>(len));
    }

    static uint64_t swap_bytes(uint64_t val, uint64_t len) {
        switch (len) {
            case 2: return __builtin_bswap16(static_cast<uint16_t>(val));
            case 4: return __builtin_bswap32(static_cast<uint32_t>(val));
            case 8: return __builtin_bswap64(val);
            default: return val;
        }
    }

    // ── XML download from camera ────────────────────────────────────────

    std::string download_device_xml() {
        // Read manifest table address
        uint64_t manifest_addr = read_reg<uint64_t>(ABRM_MANIFEST_TABLE_ADDRESS);

        // Read entry count
        uint64_t entry_count = 0;
        read_reg(manifest_addr, &entry_count, sizeof(entry_count));
        if (entry_count == 0 || entry_count > 64)
            throw std::runtime_error("Invalid manifest entry count: " + std::to_string(entry_count));

        // Read first manifest entry (72 bytes at manifest_addr + 8)
        uint8_t entry_buf[72] = {};
        read_reg(manifest_addr + 8, entry_buf, sizeof(entry_buf));

        uint64_t file_info    = read_le_u64(entry_buf + 0x00);
        uint64_t file_offset  = read_le_u64(entry_buf + 0x08);
        uint64_t file_size_64 = read_le_u64(entry_buf + 0x10);
        uint32_t file_size    = static_cast<uint32_t>(file_size_64);
        uint8_t  compression  = (file_info >> 48) & 0xFF;

        if (file_size == 0 || file_size > 16 * 1024 * 1024)
            throw std::runtime_error("Invalid manifest file size: " + std::to_string(file_size));

        // Download payload
        std::vector<uint8_t> payload(file_size);
        read_reg(file_offset, payload.data(), file_size);

        if (compression == 0) {
            // Uncompressed XML
            return std::string(reinterpret_cast<char*>(payload.data()), file_size);
        }

        if (compression == 1) {
            // ZIP compressed - extract using minizip-style inline ZIP parsing
            return extract_xml_from_zip(payload);
        }

        throw std::runtime_error("Unsupported manifest compression: " + std::to_string(compression));
    }

    // Extract XML content from ZIP payload using libzip.
    static std::string extract_xml_from_zip(const std::vector<uint8_t>& zip_data) {
        zip_error_t src_error;
        zip_error_init(&src_error);

        zip_source_t* source = zip_source_buffer_create(
            zip_data.data(), zip_data.size(), 0, &src_error);
        if (!source) {
            std::string msg = zip_error_strerror(&src_error);
            zip_error_fini(&src_error);
            throw std::runtime_error("zip_source_buffer_create failed: " + msg);
        }
        zip_error_fini(&src_error);

        zip_error_t open_error;
        zip_error_init(&open_error);
        zip_t* archive = zip_open_from_source(source, ZIP_RDONLY, &open_error);
        if (!archive) {
            std::string msg = zip_error_strerror(&open_error);
            zip_error_fini(&open_error);
            zip_source_free(source);
            throw std::runtime_error("zip_open_from_source failed: " + msg);
        }
        zip_error_fini(&open_error);

        const zip_int64_t entry_count = zip_get_num_entries(archive, 0);
        if (entry_count <= 0) {
            zip_close(archive);
            throw std::runtime_error("ZIP archive is empty");
        }

        zip_uint64_t selected_index = static_cast<zip_uint64_t>(-1);
        for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(entry_count); ++i) {
            const char* name = zip_get_name(archive, i, 0);
            if (!name)
                continue;

            std::string file_name(name);
            if (!file_name.empty() && file_name.back() == '/')
                continue;

            std::string ext = std::filesystem::path(file_name).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".xml") {
                selected_index = i;
                break;
            }

            if (selected_index == static_cast<zip_uint64_t>(-1))
                selected_index = i;
        }

        if (selected_index == static_cast<zip_uint64_t>(-1)) {
            zip_close(archive);
            throw std::runtime_error("No file entries found in ZIP archive");
        }

        zip_file_t* file = zip_fopen_index(archive, selected_index, 0);
        if (!file) {
            std::string msg = zip_strerror(archive);
            zip_close(archive);
            throw std::runtime_error("zip_fopen_index failed: " + msg);
        }

        std::string xml_content;
        std::vector<char> buffer(8192);
        while (true) {
            zip_int64_t n = zip_fread(file, buffer.data(), buffer.size());
            if (n < 0) {
                std::string msg = zip_file_strerror(file);
                zip_fclose(file);
                zip_close(archive);
                throw std::runtime_error("zip_fread failed: " + msg);
            }
            if (n == 0)
                break;
            xml_content.append(buffer.data(), static_cast<size_t>(n));
        }

        zip_fclose(file);
        zip_close(archive);

        if (xml_content.empty())
            throw std::runtime_error("XML entry in ZIP archive is empty");

        return xml_content;
    }

    // Low-level memory read via ioctl (chunked for GenCP)
    static bool read_mem(int fd, uint64_t address, void* buffer, uint32_t size) {
        uint8_t* dst = static_cast<uint8_t*>(buffer);
        uint32_t offset = 0;

        while (offset < size) {
            uint32_t chunk = std::min(size - offset, GENCP_READ_CHUNK);
            uint32_t bytes_read = 0;
            u3v_read_memory rd = {
                .address = address + offset,
                .u_buffer = dst + offset,
                .transfer_size = chunk,
                .u_bytes_read = &bytes_read,
            };
            if (ioctl(fd, U3V_IOCTL_READ, &rd) != 0 || bytes_read != chunk)
                return false;
            offset += chunk;
        }
        return true;
    }

    // Low-level memory write via ioctl
    static bool write_mem(int fd, uint64_t address, const void* buffer, uint32_t size) {
        const uint8_t* src = static_cast<const uint8_t*>(buffer);
        uint32_t offset = 0;

        while (offset < size) {
            uint32_t chunk = std::min(size - offset, GENCP_READ_CHUNK);
            uint32_t bytes_written = 0;
            u3v_write_memory wr = {
                .address = address + offset,
                .u_buffer = src + offset,
                .transfer_size = chunk,
                .u_bytes_written = &bytes_written,
            };
            if (ioctl(fd, U3V_IOCTL_WRITE, &wr) != 0 || bytes_written != chunk)
                return false;
            offset += chunk;
        }
        return true;
    }

    std::string read_abrm_string(uint64_t offset) {
        ensure_open();
        char buf[ABRM_STRING_LEN] = {};
        if (!read_mem(fd_, offset, buf, ABRM_STRING_LEN))
            throw std::runtime_error("Failed to read ABRM string at 0x" + to_hex(offset));
        return std::string(buf, strnlen(buf, ABRM_STRING_LEN));
    }

    void cleanup_streaming() {
        // Cancel pending transfers
        ioctl(fd_, U3V_IOCTL_CANCEL_ALL_BUFS);

        // Unconfigure each buffer
        for (auto& buf : buffers_) {
            if (buf.handle != 0) {
                u3v_unconfigure_buffer ub = { .buffer_handle = buf.handle };
                ioctl(fd_, U3V_IOCTL_UNCONFIGURE_BUF, &ub);
            }
        }
        buffers_.clear();

        // Unconfigure stream
        ioctl(fd_, U3V_IOCTL_UNCONFIGURE_STREAM);
    }

    static std::string to_hex(uint64_t val) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%08lX", static_cast<unsigned long>(val));
        return buf;
    }

    static uint64_t make_bit_mask(uint8_t lsb, uint8_t msb) {
        if (msb < lsb)
            throw std::runtime_error("Invalid bitfield range: msb < lsb");

        uint8_t width = static_cast<uint8_t>(msb - lsb + 1);
        if (width == 0 || width > 64)
            throw std::runtime_error("Invalid bitfield width: " + std::to_string(width));

        if (width == 64) {
            if (lsb != 0)
                throw std::runtime_error("Invalid 64-bit bitfield offset");
            return ~0ULL;
        }

        return ((1ULL << width) - 1ULL) << lsb;
    }

    static uint64_t read_le_u64(const uint8_t* p) {
        return static_cast<uint64_t>(p[0])
             | (static_cast<uint64_t>(p[1]) << 8)
             | (static_cast<uint64_t>(p[2]) << 16)
             | (static_cast<uint64_t>(p[3]) << 24)
             | (static_cast<uint64_t>(p[4]) << 32)
             | (static_cast<uint64_t>(p[5]) << 40)
             | (static_cast<uint64_t>(p[6]) << 48)
             | (static_cast<uint64_t>(p[7]) << 56);
    }
};

} // namespace u3v
