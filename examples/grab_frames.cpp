/*
 * grab_frames.cpp - Find camera, configure via GenICam features, grab frames
 *
 * Demonstrates both synchronous get_frame() and async grabber with ring buffer.
 *
 * Build: cmake --build build
 * Usage: sudo ./build/u3v_example [/dev/u3vX] [frame_count]
 */

#include "u3v_camera.hpp"
#include <cstdio>
#include <cstdlib>
#include <thread>

int main(int argc, char* argv[]) {
    const char* device = argc > 1 ? argv[1] : "";
    int frame_count = argc > 2 ? std::atoi(argv[2]) : 10;

    try {
        // List available cameras
        auto devices = u3v::Camera::enumerate();
        printf("Found %zu U3V device(s):\n", devices.size());
        for (const auto& d : devices)
            printf("  %s\n", d.c_str());

        // Open camera
        u3v::Camera cam;
        if (device[0] != '\0')
            cam.open(device);
        else
            cam.open();

        printf("\nConnected to: %s\n", cam.device_path().c_str());
        printf("  Manufacturer: %s\n", cam.manufacturer_name().c_str());
        printf("  Model:        %s\n", cam.model_name().c_str());
        printf("  Serial:       %s\n", cam.serial_number().c_str());

        // Load GenICam XML from camera
        printf("\nLoading GenICam XML...\n");
        cam.load_xml();

        // Search features by regex
        printf("\nExposure-related features:\n");
        for (const auto& name : cam.find_features("exposure"))
            printf("  %s\n", name.c_str());

        printf("\nGain-related features:\n");
        for (const auto& name : cam.find_features("gain"))
            printf("  %s\n", name.c_str());

        printf("\nAll *Width* and *Height* features:\n");
        for (const auto& name : cam.find_features("width|height"))
            printf("  %s\n", name.c_str());

        // Read image dimensions
        printf("\nImage configuration:\n");
        try {
            auto info = cam.describe_feature("Width");
            printf("  Width:  %ld", static_cast<long>(cam.get_feature("Width")));
            if (info.min) printf("  (min=%ld", static_cast<long>(*info.min));
            if (info.max) printf(", max=%ld", static_cast<long>(*info.max));
            if (info.inc) printf(", inc=%ld", static_cast<long>(*info.inc));
            if (info.min || info.max) printf(")");
            printf("\n");
        } catch (const std::exception& e) {
            printf("  Width: (not available: %s)\n", e.what());
        }

        try {
            printf("  Height: %ld\n", static_cast<long>(cam.get_feature("Height")));
        } catch (...) {
            printf("  Height: (not available)\n");
        }

        try {
            printf("  ExposureTime: %ld\n", static_cast<long>(cam.get_feature("ExposureTime")));
        } catch (...) {
            printf("  ExposureTime: (not available)\n");
        }

        try {
            printf("  PixelFormat: %s\n", cam.get_enum_feature("PixelFormat").c_str());
        } catch (...) {
            printf("  PixelFormat: (not available)\n");
        }

        // Calculate buffer size
        uint64_t image_size = 1920 * 1080;
        try {
            int64_t w = cam.get_feature("Width");
            int64_t h = cam.get_feature("Height");
            image_size = static_cast<uint64_t>(w * h);
        } catch (...) {}

        // ── Method 1: Synchronous grab ──────────────────────────────────

        printf("\n=== Synchronous grab (%d frames) ===\n", frame_count / 2);
        cam.start_streaming(image_size, 4);
        try { cam.execute_command("AcquisitionStart"); } catch (...) {}

        int received = 0;
        for (int i = 0; i < frame_count / 2; i++) {
            auto frame = cam.get_frame(std::chrono::milliseconds(2000));
            if (frame) {
                received++;
                printf("  Frame %3d: block_id=%4lu  %5zu bytes  status=0x%04X\n",
                       i, static_cast<unsigned long>(frame->block_id),
                       frame->image_data.size(), frame->status);
            } else {
                printf("  Frame %3d: timeout\n", i);
            }
        }

        // ── Method 2: Async grabber with ring buffer ────────────────────

        printf("\n=== Async grabber (%d frames, queue=5) ===\n", frame_count / 2);
        cam.start_grabber(5);  // background thread, ring buffer of 5 frames

        for (int i = 0; i < frame_count / 2; i++) {
            // Simulate processing delay - grabber keeps capturing in background
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // wait_frame() blocks until a frame is available (or timeout)
            auto frame = cam.wait_frame(std::chrono::milliseconds(2000));
            if (frame) {
                received++;
                printf("  Frame %3d: block_id=%4lu  %5zu bytes  "
                       "(queue=%zu, grabbed=%lu, dropped=%lu)\n",
                       i, static_cast<unsigned long>(frame->block_id),
                       frame->image_data.size(),
                       cam.frame_queue_size(),
                       static_cast<unsigned long>(cam.frames_grabbed()),
                       static_cast<unsigned long>(cam.frames_dropped()));
            } else {
                printf("  Frame %3d: timeout\n", i);
            }
        }

        cam.stop_grabber();
        printf("Grabber stats: grabbed=%lu, dropped=%lu\n",
               static_cast<unsigned long>(cam.frames_grabbed()),
               static_cast<unsigned long>(cam.frames_dropped()));

        try { cam.execute_command("AcquisitionStop"); } catch (...) {}
        cam.stop_streaming();

        printf("\nDone: %d frames received total\n", received);

    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    return 0;
}
