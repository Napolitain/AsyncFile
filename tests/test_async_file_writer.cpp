#include "gtest/gtest.h"
#include <AsyncFile/AsyncFileReader.hpp>
#include <AsyncFile/AsyncFileWriter.hpp>
#include <print>
#include <string>
#include <chrono>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <thread> // For std::this_thread::sleep_for

namespace fs = std::filesystem;

// Helper function to read file content into a string
std::string readFileContent(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for reading: " + filename);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

TEST(AsyncFileTest, ContentIdentity) {
    const std::string testFilenameAsync = "test_async_write.bin";
    const std::string testFilenameOfstream = "test_ofstream_write.bin";
    const std::string testContent = "This is some test content for verifying file identity. It's not too large.";

    // --- Write with AsyncFileWriter ---
    AsyncFileWriter asyncWriter(testFilenameAsync);
    asyncWriter.writeAsync(testContent.data(), testContent.length(), 0);
    asyncWriter.wait();

    // --- Write with std::ofstream ---
    std::ofstream ofs(testFilenameOfstream, std::ios::binary | std::ios::out);
    if (!ofs.is_open()) {
        FAIL() << "Could not open file for ofstream writing: " << testFilenameOfstream;
    }
    ofs.write(testContent.data(), testContent.length());
    ofs.close();

    // --- Verify Content ---
    std::string contentAsync = readFileContent(testFilenameAsync);
    std::string contentOfstream = readFileContent(testFilenameOfstream);

    ASSERT_EQ(contentAsync, testContent);
    ASSERT_EQ(contentOfstream, testContent);
    ASSERT_EQ(contentAsync, contentOfstream);

    // --- Clean up ---
    fs::remove(testFilenameAsync);
    fs::remove(testFilenameOfstream);
}

TEST(AsyncFileTest, AsyncVsOfstreamPerformanceAndNonBlocking) {
    const std::string testFilenameAsync = "test_async_perf.bin";
    const std::string testFilenameOfstream = "test_ofstream_perf.bin";
    constexpr auto size = 10 * 1024 * 1024;
    const std::string testContent(size, 'B'); // 10 MB of data

    // --- AsyncFileWriter: Non-blocking behavior ---
    AsyncFileWriter asyncWriter(testFilenameAsync);

    const auto async_start_init = std::chrono::high_resolution_clock::now();
    asyncWriter.writeAsync(testContent.data(), testContent.length(), 0);
    const auto async_end_init = std::chrono::high_resolution_clock::now();

    // Simulate some CPU-bound work or other tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Short sleep

    const auto async_start_wait = std::chrono::high_resolution_clock::now();
    asyncWriter.wait();
    const auto async_end_wait = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> async_init_duration = async_end_init - async_start_init;
    const std::chrono::duration<double> async_total_duration = async_end_wait - async_start_init;

    std::println("\n--- AsyncFileWriter Performance ---\n");
    std::println("writeAsync call returned in: {} ms (should be very fast).", async_init_duration.count() * 1000);
    std::println("Total async operation (init + work + wait) took: {} seconds.", async_total_duration.count());

    // Assert that writeAsync returns very quickly
    ASSERT_LT(async_init_duration.count(), 0.01); // e.g., less than 10 ms

    // --- std::ofstream: Blocking behavior ---
    const auto ofstream_start = std::chrono::high_resolution_clock::now();
    std::ofstream ofs(testFilenameOfstream, std::ios::binary | std::ios::out);
    if (!ofs.is_open()) {
        FAIL() << "Could not open file for ofstream writing: " << testFilenameOfstream;
    }
    ofs.write(testContent.data(), testContent.length());
    ofs.close();
    const auto ofstream_end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> ofstream_duration = ofstream_end - ofstream_start;

    std::println("\n--- std::ofstream Performance ---");
    std::println("std::ofstream write took: {} seconds.", ofstream_duration.count());

    // Assert that ofstream write is blocking and takes longer than async_init_duration
    ASSERT_GT(ofstream_duration.count(), async_init_duration.count());
    // Also, assert that the total async operation (including wait) is comparable or faster than ofstream
    // This might vary based on system, but generally async should be efficient.
    // For a 10MB file, it should definitely take more than 10ms.
    ASSERT_GT(ofstream_duration.count(), 0.01);

    // --- Verify File Sizes ---
    ASSERT_EQ(fs::file_size(testFilenameAsync), testContent.length());
    ASSERT_EQ(fs::file_size(testFilenameOfstream), testContent.length());

    // --- Clean up ---
    fs::remove(testFilenameAsync);
    fs::remove(testFilenameOfstream);
}
