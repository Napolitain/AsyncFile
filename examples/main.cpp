#include "AsyncFile/AsyncFileReader.hpp"
#include "AsyncFile/AsyncFileWriter.hpp"

#include <print>
#include <string_view>
#include <thread>
#include <chrono>
#include <cassert>

void print_timing(const std::string &name, const std::chrono::steady_clock::time_point start,
                  const std::chrono::steady_clock::time_point end) {
	std::println("{} took {} us", name, std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

int main() {
	const std::string test_data = "Hello, Async World! This is a test to verify integrity.";
	std::println("Test data: \"{}\"", test_data);
	std::println("----------------------------------------");

	// --- Writing ---
	{
		AsyncFileWriter writer("test.txt", false);

		const auto start_async_call = std::chrono::steady_clock::now();
		writer.writeAsync(test_data.data(), test_data.size());
		const auto end_async_call = std::chrono::steady_clock::now();
		print_timing("writeAsync() call", start_async_call, end_async_call);

		std::println("Doing other work while writing...");
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate other work

		const auto start_wait = std::chrono::steady_clock::now();
		writer.wait();
		const auto end_wait = std::chrono::steady_clock::now();
		print_timing("writer.wait()", start_wait, end_wait);
		std::println("Write complete.");
	}

	std::println("----------------------------------------");

	// --- Reading ---
	std::string content; {
		AsyncFileReader reader("test.txt");
		const auto start_async_call = std::chrono::steady_clock::now();
		reader.readAsync();
		const auto end_async_call = std::chrono::steady_clock::now();
		print_timing("readAsync() call", start_async_call, end_async_call);

		std::println("Doing other work while reading...");
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate other work

		const auto start_wait = std::chrono::steady_clock::now();
		content = reader.waitAndGetResult();
		const auto end_wait = std::chrono::steady_clock::now();
		print_timing("reader.waitAndGetResult()", start_wait, end_wait);
	}

	std::println("----------------------------------------");
	std::println("Read content: \"{}\"", content);

	// --- Verification ---
	if (content == test_data) {
		std::println("\nSUCCESS: Read content matches written data.");
	} else {
		std::println("\nFAILURE: Read content does not match written data.");
	}

	return 0;
}
