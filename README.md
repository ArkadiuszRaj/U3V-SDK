# U3V-SDK

C++ SDK for USB3 Vision cameras on Linux. Provides a simple API for camera discovery, GenICam register access, and image streaming over the [usb3vision](https://github.com/ArkadiuszRaj/usb3vision) kernel driver.

## Architecture

```
┌─────────────────────────────────────────┐
│            User Application             │
├─────────────────────────────────────────┤
│  U3VCamera   (include/u3v_camera.hpp)   │
│  - find camera by serial/model/index    │
│  - read/write GenICam registers         │
│  - start/stop streaming, get frames     │
├─────────────────────────────────────────┤
│  GenICam XML Parser  (include/xml_parser)│
│  - NodeMap, NodeInfo, MathCalc          │
│  - parses device feature descriptors    │
├─────────────────────────────────────────┤
│  usb3vision kernel driver  (/dev/u3vX)  │
│  - USB bulk transfers, buffer mgmt      │
│  - ioctl interface (read/write/stream)  │
└─────────────────────────────────────────┘
```

## Components

| Directory | Description |
|---|---|
| `include/u3v_camera.hpp` | Main SDK class - camera discovery, register I/O, streaming |
| `include/xml_parser/` | GenICam XML parser - NodeMap, NodeInfo, MathCalc, NodeKindTraits |
| `include/xml_parser/tools/` | Standalone utilities - XML dumper, register scanner, XML loader |
| `import/usb3vision/` | Linux kernel driver (git submodule) |
| `import/pugixml/` | pugixml XML library (git submodule) |
| `examples/` | Example applications |

## Prerequisites

- Linux with the `usb3vision` kernel module loaded
- C++20 compiler (GCC 12+ or Clang 15+)
- CMake 3.20+
- A USB3 Vision compliant camera

### Kernel driver setup

```bash
# Clone with submodules
git clone --recursive https://github.com/ArkadiuszRaj/u3v-sdk.git
cd u3v-sdk

# Build and load the kernel driver
cd import/usb3vision
make
sudo insmod u3v.ko

# Verify camera detected
ls /dev/u3v*
```

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Quick start

```cpp
#include "u3v_camera.hpp"

int main() {
    u3v::Camera cam;
    cam.open();
    cam.load_xml();  // download & parse GenICam XML from camera

    // Read/write features by GenICam name - no raw addresses needed
    int64_t width = cam.get_feature("Width");
    int64_t height = cam.get_feature("Height");
    cam.set_feature("ExposureTime", 10000);

    // Enumerations by name
    cam.set_enum_feature("PixelFormat", "Mono8");
    std::string fmt = cam.get_enum_feature("PixelFormat");  // "Mono8"

    // Inspect a feature: limits, access mode, current value
    auto info = cam.describe_feature("ExposureTime");
    // info.min, info.max, info.inc, info.unit, info.access ...

    // Stream images
    cam.start_streaming(width * height, 4);
    cam.execute_command("AcquisitionStart");

    for (int i = 0; i < 100; i++) {
        auto frame = cam.get_frame(std::chrono::milliseconds(1000));
        if (frame)
            printf("Frame %d: %zu bytes\n", i, frame->image_data.size());
    }

    cam.execute_command("AcquisitionStop");
    cam.stop_streaming();
    return 0;
}
```

## API overview

### Camera discovery

```cpp
u3v::Camera cam;

// Open first available camera
cam.open();

// Open by device path
cam.open("/dev/u3v0");

// Open by serial number
cam.open_by_serial("ABC123");
```

### GenICam features (by name)

```cpp
// Load GenICam XML from camera (or from a local file)
cam.load_xml();               // download from camera
cam.load_xml_file("cam.xml"); // or load from file

// List all features
for (const auto& name : cam.list_features())
    printf("  %s\n", name.c_str());

// Search features by regex (case-insensitive)
cam.find_features("exposure");        // -> {"ExposureAuto", "ExposureTime", ...}
cam.find_features("width|height");    // -> {"Height", "HeightMax", "Width", "WidthMax"}
cam.find_features("pixel.*format");   // -> {"PixelFormat", "PixelFormatInfoSelector"}

// Read / write integer features
int64_t exposure = cam.get_feature("ExposureTime");
cam.set_feature("ExposureTime", 20000);

// Enumeration features
std::string mode = cam.get_enum_feature("TriggerMode");
cam.set_enum_feature("TriggerMode", "Off");

// Execute commands
cam.execute_command("AcquisitionStart");
cam.execute_command("AcquisitionStop");

// Inspect a feature: limits, type, access mode, enum entries
auto info = cam.describe_feature("Gain");
// info.kind      - NodeKind::Integer, Float, Enumeration, etc.
// info.access    - AccessMode::RO, RW, WO
// info.value     - current value (read from device)
// info.min/max/inc - value range
// info.unit      - "us", "dB", etc.
// info.enum_entries - [(name, value), ...]
```

### Raw register access

```cpp
// Read a 4-byte register
uint32_t value;
cam.read_reg(address, &value, sizeof(value));

// Write a 4-byte register
uint32_t new_val = 42;
cam.write_reg(address, &new_val, sizeof(new_val));

// Read ABRM string registers (model, serial, etc.)
std::string model = cam.model_name();
std::string serial = cam.serial_number();
```

### Streaming (synchronous)

```cpp
cam.start_streaming(image_buffer_size, buffer_count);

// get_frame() blocks until a frame arrives (or timeout)
auto frame = cam.get_frame(std::chrono::milliseconds(500));
if (frame) {
    // frame->image_data   - raw pixel data (std::vector<uint8_t>)
    // frame->block_id     - U3V block ID
    // frame->payload_type - U3V_IMAGE, U3V_CHUNK, etc.
    // frame->status       - trailer status (0 = OK)
}

cam.stop_streaming();
```

### Streaming (async grabber with ring buffer)

For real-time applications where you need the latest frame without blocking:

```cpp
cam.start_streaming(image_buffer_size, 4);
cam.execute_command("AcquisitionStart");

// Background thread grabs frames into a ring buffer (queue_size=5)
cam.start_grabber(5);

while (running) {
    // wait_frame() blocks until a frame is ready (or timeout)
    auto frame = cam.wait_frame(std::chrono::milliseconds(1000));
    if (frame)
        process(frame->image_data);

    // Or non-blocking: pop_frame() returns nullopt if empty
    // auto frame = cam.pop_frame();
}

cam.stop_grabber();
printf("grabbed=%lu, dropped=%lu\n", cam.frames_grabbed(), cam.frames_dropped());

cam.execute_command("AcquisitionStop");
cam.stop_streaming();
```

When the queue is full, the oldest frame is dropped (newest-wins policy).
`pop_frame()` returns only the latest frame and clears the queue.
`wait_frame()` blocks until at least one frame is available.

## Examples

### Konfiguracja ekspozycji i rozdzielczosci

```cpp
#include "u3v_camera.hpp"

int main() {
    u3v::Camera cam;
    cam.open();
    cam.load_xml();

    // Sprawdz dostepne tryby ekspozycji
    for (const auto& name : cam.find_features("exposure"))
        printf("  %s\n", name.c_str());

    // Ustaw tryb manualny jesli jest enumem
    try {
        cam.set_enum_feature("ExposureAuto", "Off");
    } catch (...) {}

    // Sprawdz limity ekspozycji
    auto info = cam.describe_feature("ExposureTime");
    printf("ExposureTime: min=%ld, max=%ld, inc=%ld, unit=%s\n",
           info.min ? static_cast<long>(*info.min) : 0,
           info.max ? static_cast<long>(*info.max) : 0,
           info.inc ? static_cast<long>(*info.inc) : 0,
           info.unit.value_or("?").c_str());

    // Ustaw 10ms ekspozycji
    cam.set_feature("ExposureTime", 10000);

    // Ustaw ROI (Region of Interest)
    cam.set_feature("Width", 1280);
    cam.set_feature("Height", 720);
    cam.set_feature("OffsetX", 0);
    cam.set_feature("OffsetY", 0);

    printf("Configured: %ldx%ld @ %ld us exposure\n",
           static_cast<long>(cam.get_feature("Width")),
           static_cast<long>(cam.get_feature("Height")),
           static_cast<long>(cam.get_feature("ExposureTime")));

    return 0;
}
```

### Synchroniczne przechwytywanie ramek

```cpp
#include "u3v_camera.hpp"

int main() {
    u3v::Camera cam;
    cam.open();
    cam.load_xml();

    int64_t w = cam.get_feature("Width");
    int64_t h = cam.get_feature("Height");

    cam.start_streaming(static_cast<uint64_t>(w * h), 4);
    cam.execute_command("AcquisitionStart");

    // get_frame() blokuje az do otrzymania ramki (lub timeout)
    for (int i = 0; i < 100; i++) {
        auto frame = cam.get_frame(std::chrono::milliseconds(2000));
        if (frame) {
            printf("Frame %d: %zu bytes, block_id=%lu, status=0x%04X\n",
                   i, frame->image_data.size(),
                   static_cast<unsigned long>(frame->block_id),
                   frame->status);

            // frame->image_data zawiera surowe piksele
            // np. zapisz do pliku:
            // fwrite(frame->image_data.data(), 1, frame->image_data.size(), f);
        }
    }

    cam.execute_command("AcquisitionStop");
    cam.stop_streaming();  // zwalnia bufory i zamyka kanal USB
    return 0;
}
```

### Asynchroniczny grabber z ring bufferem

Gdy przetwarzanie ramek trwa dluzej niz okres miedzy ramkami,
watek w tle zapewnia ze zawsze mamy najswiezsza ramke:

```cpp
#include "u3v_camera.hpp"
#include <thread>
#include <atomic>

std::atomic<bool> running{true};

void process_frame(const u3v::Frame& frame) {
    // Czasochlonne przetwarzanie obrazu...
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("Processed block_id=%lu (%zu bytes)\n",
           static_cast<unsigned long>(frame.block_id),
           frame.image_data.size());
}

int main() {
    u3v::Camera cam;
    cam.open();
    cam.load_xml();

    int64_t w = cam.get_feature("Width");
    int64_t h = cam.get_feature("Height");

    cam.start_streaming(static_cast<uint64_t>(w * h), 4);
    cam.execute_command("AcquisitionStart");

    // Watek w tle przechwytuje ramki do ring buffera (max 5)
    cam.start_grabber(5);

    while (running) {
        // wait_frame() blokuje do pojawienia sie ramki
        auto frame = cam.wait_frame(std::chrono::milliseconds(1000));
        if (frame)
            process_frame(*frame);

        // Albo non-blocking: pop_frame() zwraca najnowsza lub nullopt
        // auto frame = cam.pop_frame();
    }

    cam.stop_grabber();

    // Statystyki: ile ramek przechwycono, ile porzucono (bo kolejka pelna)
    printf("Stats: grabbed=%lu, dropped=%lu\n",
           static_cast<unsigned long>(cam.frames_grabbed()),
           static_cast<unsigned long>(cam.frames_dropped()));

    cam.execute_command("AcquisitionStop");
    cam.stop_streaming();
    return 0;
}
```

### Przeglad wszystkich cech kamery (feature dump)

```cpp
#include "u3v_camera.hpp"

int main() {
    u3v::Camera cam;
    cam.open();
    cam.load_xml();

    printf("Camera: %s %s (S/N: %s)\n",
           cam.manufacturer_name().c_str(),
           cam.model_name().c_str(),
           cam.serial_number().c_str());

    printf("\n--- All features ---\n");
    for (const auto& name : cam.list_features()) {
        try {
            auto info = cam.describe_feature(name);
            const char* tag = kindToTag(info.kind);
            printf("%-30s [%-12s]", name.c_str(), tag ? tag : "?");

            if (info.access) {
                switch (*info.access) {
                    case AccessMode::RO: printf(" RO"); break;
                    case AccessMode::RW: printf(" RW"); break;
                    case AccessMode::WO: printf(" WO"); break;
                }
            }

            if (info.value)
                printf("  val=%-10ld", static_cast<long>(*info.value));
            if (info.min)
                printf("  min=%-10ld", static_cast<long>(*info.min));
            if (info.max)
                printf("  max=%-10ld", static_cast<long>(*info.max));
            if (info.unit)
                printf("  [%s]", info.unit->c_str());

            if (!info.enum_entries.empty()) {
                printf("  {");
                for (size_t i = 0; i < info.enum_entries.size(); i++) {
                    if (i > 0) printf(", ");
                    printf("%s=%lu", info.enum_entries[i].first.c_str(),
                           static_cast<unsigned long>(info.enum_entries[i].second));
                }
                printf("}");
            }

            printf("\n");
        } catch (const std::exception& e) {
            printf("%-30s  ERROR: %s\n", name.c_str(), e.what());
        }
    }

    return 0;
}
```

### Surowy dostep do rejestrow (bez XML)

```cpp
#include "u3v_camera.hpp"

int main() {
    u3v::Camera cam;
    cam.open();

    // Odczyt ABRM string rejestrow
    printf("Manufacturer: %s\n", cam.manufacturer_name().c_str());
    printf("Model:        %s\n", cam.model_name().c_str());
    printf("Serial:       %s\n", cam.serial_number().c_str());
    printf("GenCP:        0x%08X\n", cam.gencp_version());

    // Odczyt/zapis typowany
    uint64_t sbrm = cam.read_reg<uint64_t>(0x001D8);  // SBRM address
    printf("SBRM address: 0x%08lX\n", static_cast<unsigned long>(sbrm));

    // Odczyt surowy (dowolny rozmiar)
    uint8_t buf[256];
    cam.read_reg(0x00000, buf, sizeof(buf));

    // Zapis surowy
    uint32_t val = 42;
    cam.write_reg(0x12340, &val, sizeof(val));

    return 0;
}
```

## Tools

Standalone utilities in `include/xml_parser/tools/`:

| Tool | Description |
|---|---|
| `dump_device_xml` | Download GenICam XML descriptor from camera |
| `scanner` | Brute-force register scanner |
| `load_xml` | Parse and inspect GenICam XML files |

```bash
# Dump camera XML
sudo ./dump_device_xml /dev/u3v0

# Scan registers
sudo ./scanner /dev/u3v0 0x10000

# Parse XML
./load_xml camera.xml
```

## License

The kernel driver (`import/usb3vision`) is licensed under GPLv2.
USB3 Vision is a trademark of the AIA.
