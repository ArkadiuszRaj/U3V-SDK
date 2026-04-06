/*
 * dump_device_xml.cpp - Download GenICam XML from a USB3 Vision camera
 *
 * Reads the manifest table via the u3v kernel driver's ioctl interface,
 * downloads the XML payload, and saves it as a ZIP file.
 *
 * Build: Part of xml_parser CMake project, or standalone:
 *   g++ -std=c++20 -O2 -o dump_device_xml dump_device_xml.cpp
 *
 * Usage: sudo ./dump_device_xml [/dev/u3vX]
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#define U3V_MAGIC 0x5D
#define U3V_IOCTL_READ _IOWR(U3V_MAGIC, 1, struct u3v_read_memory)

struct u3v_read_memory {
    __u64 address;
    void* u_buffer;
    __u32 transfer_size;
    __u32* u_bytes_read;
};

static bool readmem(int fd, uint64_t addr, void* buf, uint32_t size) {
    __u32 bytes_read = 0;
    struct u3v_read_memory rd = {
        .address = addr,
        .u_buffer = buf,
        .transfer_size = size,
        .u_bytes_read = &bytes_read
    };

    int ret = ioctl(fd, U3V_IOCTL_READ, &rd);
    return (ret == 0) && (bytes_read == size);
}

int main(int argc, char* argv[]) {
    const char* dev = "/dev/u3v0";
    if (argc > 1) dev = argv[1];

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror("open");
        fprintf(stderr, "Usage: %s [/dev/u3vX]\n", argv[0]);
        return 1;
    }

    // Read MANIFEST_TABLE_ADDRESS from ABRM at offset 0x1D0
    uint64_t manifest_addr = 0;
    if (!readmem(fd, 0x01D0, &manifest_addr, sizeof(manifest_addr))) {
        fprintf(stderr, "Failed to read MANIFEST_TABLE_ADDRESS\n");
        close(fd);
        return 2;
    }
    printf("MANIFEST_TABLE_ADDRESS = 0x%08lx\n", (unsigned long)manifest_addr);

    // Read entry count (first 8 bytes of manifest table)
    uint64_t entry_count = 0;
    if (!readmem(fd, manifest_addr, &entry_count, sizeof(entry_count))) {
        fprintf(stderr, "Failed to read manifest entry count\n");
        close(fd);
        return 3;
    }
    printf("Manifest entries     = %lu\n", (unsigned long)entry_count);

    if (entry_count == 0 || entry_count > 64) {
        fprintf(stderr, "Invalid entry count: %lu\n", (unsigned long)entry_count);
        close(fd);
        return 3;
    }

    // Read first manifest entry (starts at manifest_addr + 8)
    // GenCP manifest entry: file_info(8) + register_address(8) + file_size(8) + sha1(20) + pad(28) = 72 bytes
    uint8_t entry_buf[72];
    if (!readmem(fd, manifest_addr + 8, entry_buf, sizeof(entry_buf))) {
        fprintf(stderr, "Failed to read manifest entry\n");
        close(fd);
        return 3;
    }

    uint64_t file_info    = *(uint64_t*)(entry_buf + 0x00);
    uint64_t file_offset  = *(uint64_t*)(entry_buf + 0x08);
    uint64_t file_size_64 = *(uint64_t*)(entry_buf + 0x10);
    uint32_t file_size    = static_cast<uint32_t>(file_size_64);

    uint8_t file_type    = (file_info >> 40) & 0xFF;
    uint8_t compression  = (file_info >> 48) & 0xFF;

    printf("File type            = 0x%02x\n", file_type);
    printf("Compression          = 0x%02x (%s)\n", compression,
           compression == 0 ? "None" : compression == 1 ? "ZIP" : "Unknown");
    printf("File offset          = 0x%08lx\n", (unsigned long)file_offset);
    printf("File size            = %u bytes\n", file_size);

    if (file_size == 0 || file_size > 16 * 1024 * 1024) {
        fprintf(stderr, "Invalid file size\n");
        close(fd);
        return 4;
    }

    // Download the XML/ZIP payload in chunks
    uint8_t* payload = new uint8_t[file_size];
    uint32_t offset = 0;
    uint32_t chunk = 512; // safe GenCP read size

    while (offset < file_size) {
        uint32_t to_read = (file_size - offset < chunk) ? (file_size - offset) : chunk;
        if (!readmem(fd, file_offset + offset, payload + offset, to_read)) {
            fprintf(stderr, "Failed to read payload at offset %u\n", offset);
            delete[] payload;
            close(fd);
            return 4;
        }
        offset += to_read;
    }

    const char* out_file = (compression == 1) ? "camera.zip" : "camera.xml";
    FILE* f = fopen(out_file, "wb");
    if (!f) {
        perror("fopen");
        delete[] payload;
        close(fd);
        return 5;
    }
    fwrite(payload, 1, file_size, f);
    fclose(f);
    delete[] payload;

    printf("Saved %s (%u bytes)\n", out_file, file_size);

    if (compression == 1) {
        printf("Extracting...\n");
        int ret = system("unzip -o camera.zip");
        if (ret != 0)
            fprintf(stderr, "Warning: unzip failed (is it installed?)\n");
    }

    close(fd);
    return 0;
}
