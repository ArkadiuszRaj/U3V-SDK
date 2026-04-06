/*
 * scanner.cpp - Brute-force register scanner for USB3 Vision cameras
 *
 * WARNING: Some cameras may have side-effects on register reads
 * (e.g. clearing event flags, triggering actions). Use with caution.
 *
 * Build: Part of xml_parser CMake project, or standalone:
 *   g++ -std=c++20 -O2 -o scanner scanner.cpp
 *
 * Usage: sudo ./scanner [/dev/u3vX] [end_address]
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>

#define U3V_MAGIC 0x5D
#define U3V_IOCTL_READ _IOWR(U3V_MAGIC, 1, struct u3v_read_memory)

struct u3v_read_memory {
    __u64 address;
    void* u_buffer;
    __u32 transfer_size;
    __u32* u_bytes_read;
};

int main(int argc, char* argv[]) {
    const char* dev = "/dev/u3v0";
    uint64_t end_addr = 0x10000;

    if (argc > 1) dev = argv[1];
    if (argc > 2) end_addr = strtoull(argv[2], nullptr, 0);

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror("open");
        fprintf(stderr, "Usage: %s [/dev/u3vX] [end_address]\n", argv[0]);
        return 1;
    }

    printf("Scanning %s registers 0x0000 - 0x%04llX\n", dev,
           (unsigned long long)end_addr);
    printf("WARNING: Register reads may have side-effects on some cameras!\n\n");

    uint8_t buffer[256];
    __u32 bytesRead = 0;

    for (__u64 addr = 0x0000; addr < end_addr; addr += 0x100) {
        memset(buffer, 0, sizeof(buffer));
        bytesRead = 0;

        struct u3v_read_memory rd = {
            .address = addr,
            .u_buffer = buffer,
            .transfer_size = 256,
            .u_bytes_read = &bytesRead
        };

        int ret = ioctl(fd, U3V_IOCTL_READ, &rd);
        if (ret == 0 && bytesRead > 0) {
            // Check if the block is all zeros
            bool all_zero = true;
            for (uint32_t i = 0; i < bytesRead; i++) {
                if (buffer[i] != 0) { all_zero = false; break; }
            }
            if (all_zero) continue;

            printf("0x%04llx: %u bytes  ", (unsigned long long)addr, bytesRead);
            for (uint32_t i = 0; i < 16 && i < bytesRead; i++)
                printf("%02X ", buffer[i]);
            printf("\n");
        }
    }

    close(fd);
    return 0;
}
