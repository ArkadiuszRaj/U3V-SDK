# U3V SDK API Reference

Complete documentation of classes, methods, parameters, and data structures for the USB3 Vision SDK.

---

## Table of Contents

1. [Core Classes](#core-classes)
2. [Data Structures](#data-structures)
3. [Camera Class Methods](#camera-class-methods)
4. [Enumerations & Constants](#enumerations--constants)
5. [Exceptions](#exceptions)

---

## Core Classes

### `u3v::Camera`

Main class for USB3 Vision camera access. Provides device discovery, GenICam feature access, register I/O, and image streaming.

#### Constructor & Destructor

```cpp
Camera()                        // Default constructor
~Camera()                       // Destructor (calls close/stop_streaming)
```

#### Non-copyable & Non-moveable

The `Camera` class deletes copy and move constructors/operators to ensure exclusive ownership of device resources.

---

## Data Structures

### `u3v::Frame`

Represents a single frame captured from the camera.

```cpp
struct Frame {
    std::vector<uint8_t> image_data;    // Raw pixel data
    uint64_t block_id = 0;              // U3V block ID from leader
    uint64_t timestamp = 0;             // Timestamp (if available)
    uint16_t payload_type = 0;          // U3V payload type (e.g. U3V_PAYLOAD_IMAGE)
    uint16_t status = 0;                // Trailer status (0 = OK)
    uint64_t valid_payload_size = 0;    // Actual data size in frame
};
```

### `Camera::FeatureInfo`

Describes a GenICam feature including type, access mode, limits, and current value.

```cpp
struct FeatureInfo {
    std::string name;                                   // Feature name (e.g. "ExposureTime")
    NodeKind kind;                                      // Node type (Integer, Float, Enumeration, etc.)
    std::optional<uint64_t> address;                    // Register address
    std::optional<uint64_t> length;                     // Register size in bytes
    std::optional<AccessMode> access;                   // Read/Write/Execute permission
    std::optional<int64_t> value;                       // Current value (read from device)
    std::optional<int64_t> min;                         // Minimum value
    std::optional<int64_t> max;                         // Maximum value
    std::optional<int64_t> inc;                         // Increment step
    std::optional<std::string> unit;                    // Unit string (e.g. "us", "dB")
    std::vector<std::pair<std::string, uint64_t>> enum_entries;  // Enumeration values
};
```

---

## Camera Class Methods

### Device Discovery & Management

#### `static std::vector<std::string> enumerate()`

List all available USB3 Vision camera devices.

**Returns:**
- `std::vector<std::string>` — Vector of device paths (e.g. `/dev/u3v0`, `/dev/u3v1`). Empty if none found.

**Example:**
```cpp
auto devices = u3v::Camera::enumerate();
for (const auto& dev : devices)
    std::cout << dev << std::endl;
```

#### `void open(const std::string& device_path = "")`

Open a camera device.

**Parameters:**
- `device_path` (optional) — Full device path (e.g. `/dev/u3v0`). If empty, opens the first available device.

**Throws:**
- `std::runtime_error` if no device path is provided and none are found, or if the device cannot be opened.

#### `void open_by_serial(const std::string& serial)`

Open a camera matching a specific serial number.

**Parameters:**
- `serial` — Serial number to match (case-sensitive).

**Throws:**
- `std::runtime_error` if no camera with that serial is found.

#### `void open_by_model(const std::string& model)`

Open a camera matching a specific model name.

**Parameters:**
- `model` — Model name to match (case-sensitive).

**Throws:**
- `std::runtime_error` if no camera with that model is found.

#### `void close()`

Close the camera device and release resources.

#### `bool is_open() const`

Check if a device is currently open.

**Returns:** `true` if open, `false` otherwise.

#### `const std::string& device_path() const`

Get the device path of the open camera.

**Returns:** Reference to the device path string (e.g. `/dev/u3v0`).

---

### Raw Register Access

#### `void read_reg(uint64_t address, void* buffer, uint32_t size)`

Read raw bytes from a device register.

**Parameters:**
- `address` — Register address.
- `buffer` — Pointer to destination buffer.
- `size` — Number of bytes to read.

**Throws:**
- `std::runtime_error` if the read fails or device is not open.

#### `void write_reg(uint64_t address, const void* buffer, uint32_t size)`

Write raw bytes to a device register.

**Parameters:**
- `address` — Register address.
- `buffer` — Pointer to source buffer.
- `size` — Number of bytes to write.

**Throws:**
- `std::runtime_error` if the write fails or device is not open.

#### `template<typename T> T read_reg(uint64_t address)`

Read a typed value from a register.

**Template Parameters:**
- `T` — Any integral type (uint8_t, uint32_t, uint64_t, etc.)

**Returns:** The value read from the register.

#### `template<typename T> void write_reg(uint64_t address, T value)`

Write a typed value to a register.

**Template Parameters:**
- `T` — Any integral type.

---

### ABRM Convenience Accessors

Read device identity from ABRM (All brooked Register Map) registers.

#### `std::string manufacturer_name()`

Get the manufacturer name (e.g. "The Imaging Source").

#### `std::string model_name()`

Get the camera model name (e.g. "USB 3 CMOS Mono Global Shutter").

#### `std::string family_name()`

Get the device family name.

#### `std::string device_version()`

Get the device firmware version.

#### `std::string serial_number()`

Get the device serial number.

#### `uint32_t gencp_version()`

Get the GenCP (Generic Control Protocol) version.

#### `uint64_t sbrm_address()`

Get the SBRM (SIRM/EIRM Base Register Map) address.

---

### GenICam XML & Feature Access

#### `void load_xml()`

Download GenICam XML descriptor from the camera and parse it.

**Must be called before using any `get_feature()` or `set_feature()` methods.**

**Throws:**
- `std::runtime_error` if the download or parsing fails.

#### `void load_xml_file(const std::string& path)`

Load and parse a GenICam XML from a local file.

**Parameters:**
- `path` — File path to the XML file.

**Throws:**
- `std::runtime_error` if the file cannot be opened or parsing fails.

#### `bool is_xml_loaded() const`

Check if XML has been loaded.

**Returns:** `true` if XML is loaded, `false` otherwise.

#### `const NodeMap& node_map() const`

Access the parsed GenICam NodeMap (for advanced use).

#### `std::vector<std::string> list_features() const`

Get all public feature names in alphabetical order.

**Throws:**
- `std::runtime_error` if XML is not loaded.

#### `std::vector<std::string> find_features(const std::string& pattern, bool case_insensitive = true) const`

Find features matching a regex pattern.

**Parameters:**
- `pattern` — Regular expression pattern (e.g. "exposure", "width|height").
- `case_insensitive` — If `true` (default), perform case-insensitive match.

**Returns:** Sorted vector of matching feature names.

**Throws:**
- `std::runtime_error` if XML is not loaded or regex is invalid.

**Examples:**
```cpp
auto features = cam.find_features("exposure");     // ExposureAuto, ExposureTime, ...
features = cam.find_features("width|height");      // Height, HeightMax, Width, WidthMax
features = cam.find_features("pixel.*format");     // PixelFormat, PixelFormatInfoSelector
```

#### `FeatureInfo describe_feature(const std::string& name)`

Get detailed information about a feature.

**Parameters:**
- `name` — Feature name (e.g. "ExposureTime").

**Returns:** `FeatureInfo` struct with type, limits, access mode, and current value.

**Throws:**
- `std::runtime_error` if the feature is not found or XML is not loaded.

#### `int64_t get_feature(const std::string& name)`

Read a feature value by name.

**Parameters:**
- `name` — Feature name.

**Returns:** The current value as a 64-bit signed integer.

**Throws:**
- `std::runtime_error` if the feature is not found, XML is not loaded, or the read fails.

**Example:**
```cpp
int64_t width = cam.get_feature("Width");
int64_t exposure = cam.get_feature("ExposureTime");
```

#### `void set_feature(const std::string& name, int64_t value)`

Write a feature value by name.

**Parameters:**
- `name` — Feature name.
- `value` — Value to write.

**Throws:**
- `std::runtime_error` if the feature is read-only, not found, XML is not loaded, or the write fails.

**Example:**
```cpp
cam.set_feature("ExposureTime", 10000);  // 10000 microseconds
cam.set_feature("Width", 1280);
```

#### `std::string get_enum_feature(const std::string& name)`

Read an Enumeration feature as a string name.

**Parameters:**
- `name` — Enumeration feature name.

**Returns:** The enum entry name (e.g. "Mono8", "Off").

**Throws:**
- `std::runtime_error` if the feature is not an enumeration or not found.

**Example:**
```cpp
std::string format = cam.get_enum_feature("PixelFormat");  // "Mono8"
std::string mode = cam.get_enum_feature("TriggerMode");    // "Off"
```

#### `void set_enum_feature(const std::string& feature_name, const std::string& entry_name)`

Write an Enumeration feature by entry name.

**Parameters:**
- `feature_name` — Enumeration feature name.
- `entry_name` — Enum value name (must match exactly).

**Throws:**
- `std::runtime_error` if the entry name is not found or the feature is not an enumeration.

**Example:**
```cpp
cam.set_enum_feature("PixelFormat", "Mono8");
cam.set_enum_feature("TriggerMode", "Off");
cam.set_enum_feature("ExposureAuto", "Continuous");
```

#### `void execute_command(const std::string& name)`

Execute a Command feature.

**Parameters:**
- `name` — Command feature name (e.g. "AcquisitionStart").

**Throws:**
- `std::runtime_error` if the feature is not a command or not found.

**Example:**
```cpp
cam.execute_command("AcquisitionStart");
cam.execute_command("AcquisitionStop");
cam.execute_command("TriggerSoftware");
```

---

### Streaming (Synchronous)

#### `void start_streaming(uint64_t image_buffer_size, uint32_t buffer_count = 4)`

Configure buffers and begin streaming.

**Parameters:**
- `image_buffer_size` — Expected maximum image payload size in bytes. Typically `width * height` for grayscale images.
- `buffer_count` — Number of buffers to allocate and queue (default 4). More buffers = less frame drops under load.

**Throws:**
- `std::runtime_error` if already streaming, buffers cannot be allocated, or ioctl fails.

#### `std::optional<Frame> get_frame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))`

Grab the next available frame (blocking with timeout).

**Parameters:**
- `timeout` — Maximum wait time for a frame. Default is 1000 ms.

**Returns:**
- `std::optional<Frame>` — Contains the frame if successful, `std::nullopt` on timeout.

**Throws:**
- `std::runtime_error` if not streaming.

**Note:** This method automatically re-queues the buffer for the next frame.

**Example:**
```cpp
cam.start_streaming(1280 * 720, 4);

for (int i = 0; i < 100; i++) {
    auto frame = cam.get_frame(std::chrono::milliseconds(2000));
    if (frame) {
        printf("Got frame: %zu bytes\n", frame->image_data.size());
        // Process frame->image_data
    }
}

cam.stop_streaming();
```

#### `void stop_streaming()`

Stop streaming, release all buffers, and stop the grabber thread if running.

#### `bool is_streaming() const`

Check if currently streaming.

**Returns:** `true` if streaming, `false` otherwise.

---

### Streaming (Asynchronous with Ring Buffer)

#### `void start_grabber(uint32_t queue_size = 5)`

Start a background thread that continuously grabs frames into a ring buffer.

**Parameters:**
- `queue_size` — Maximum number of frames to hold (default 5). Older frames are dropped when the queue is full (newest-wins policy).

**Precondition:** `start_streaming()` must be called first.

**Throws:**
- `std::runtime_error` if not streaming or grabber is already running.

**Note:** After calling `start_streaming()`, call `start_grabber()` to enable background frame capture.

#### `void stop_grabber()`

Stop the background grabber thread.

#### `bool is_grabber_running() const`

Check if the grabber thread is running.

**Returns:** `true` if running, `false` otherwise.

#### `std::optional<Frame> pop_frame()`

Pop the newest frame from the queue (non-blocking).

**Returns:**
- The latest frame if available.
- `std::nullopt` if the queue is empty.

**Note:** This method clears the queue, keeping only the latest frame.

#### `std::optional<Frame> wait_frame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))`

Wait for a frame with timeout (blocking).

**Parameters:**
- `timeout` — Maximum wait time. Default is 1000 ms.

**Returns:**
- The latest frame if one arrives within the timeout.
- `std::nullopt` on timeout.

**Note:** This method also clears the queue, keeping only the latest frame.

#### `size_t frame_queue_size() const`

Get the current number of frames in the queue.

**Returns:** Number of frames currently buffered.

#### `uint64_t frames_grabbed() const`

Get the total number of frames grabbed by the background thread (including dropped frames).

#### `uint64_t frames_dropped() const`

Get the total number of frames dropped due to queue overflow.

**Example:**
```cpp
cam.start_streaming(1280 * 720, 4);
cam.execute_command("AcquisitionStart");

// Background thread acquires frames
cam.start_grabber(5);

std::this_thread::sleep_for(std::chrono::seconds(2));

auto frame = cam.wait_frame(std::chrono::milliseconds(500));
if (frame)
    printf("Latest frame: %zu bytes\n", frame->image_data.size());

// Or non-blocking:
// auto frame = cam.pop_frame();

printf("Stats: grabbed=%lu, dropped=%lu\n",
       cam.frames_grabbed(),
       cam.frames_dropped());

cam.stop_grabber();
cam.execute_command("AcquisitionStop");
cam.stop_streaming();
```

---

## Enumerations & Constants

### U3V Protocol Constants

```cpp
constexpr uint32_t U3V_LEADER_MAGIC  = 0x4C563355;  // "U3VL"
constexpr uint32_t U3V_TRAILER_MAGIC = 0x54563355;  // "U3VT"
constexpr uint16_t U3V_PAYLOAD_IMAGE = 0x0001;
constexpr uint16_t U3V_PAYLOAD_CHUNK = 0x4000;
```

### ABRM Register Offsets

```cpp
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
```

### SBRM Register Offsets

```cpp
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
```

---

## Exceptions

The SDK throws exceptions of type `std::runtime_error` or `std::invalid_argument` for error conditions. Always check return values and wrap calls in try-catch blocks:

```cpp
try {
    u3v::Camera cam;
    cam.open();
    cam.load_xml();
    auto val = cam.get_feature("ExposureTime");
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

---

## Common Error Scenarios

| Error | Cause | Solution |
|-------|-------|----------|
| "Camera not open" | Calling method on closed device | Call `open()` first |
| "GenICam XML not loaded" | Accessing features without XML | Call `load_xml()` first |
| "No U3V cameras found" | No devices enumerated | Load `usb3vision` kernel module: `insmod u3v.ko` |
| "Feature not found" | Feature name doesn't exist | Use `find_features()` to search for available features |
| "Feature is read-only" | Attempting to write to RO feature | Check feature access with `describe_feature()` |
| "Not streaming" | Calling stream methods without streaming | Call `start_streaming()` first |
| "Already streaming" | Calling `start_streaming()` twice | Call `stop_streaming()` first |

