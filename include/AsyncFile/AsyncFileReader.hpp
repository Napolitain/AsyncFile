#pragma once

#ifdef __linux__

#include <string>
#include <liburing.h>

class AsyncFileReader {
	int fd_ = -1;
	char *buffer_ = nullptr;
	size_t buffer_size_;
	io_uring ring_;

	void cleanup();

public:
	explicit AsyncFileReader(const std::string &path, size_t buffer_size = 4096);

	// Disable copying
	AsyncFileReader(const AsyncFileReader &) = delete;

	~AsyncFileReader();

	AsyncFileReader &operator=(const AsyncFileReader &) = delete;

	// Enable moving
	AsyncFileReader(AsyncFileReader &&other) noexcept;

	AsyncFileReader &operator=(AsyncFileReader &&other) noexcept;

	// Start the async read
	void readAsync(off_t offset = 0);

	// Wait for read to complete
	std::string_view waitAndGetResult();
};

#elif _WIN32

#include <string>
#include <windows.h>

class AsyncFileReader {
	HANDLE hFile_ = INVALID_HANDLE_VALUE;
	char *buffer_ = nullptr;
	size_t buffer_size_;
	OVERLAPPED overlapped_{};

	void cleanup();

public:
	explicit AsyncFileReader(const std::string &path, size_t buffer_size = 4096);

	// Disable copying
	AsyncFileReader(const AsyncFileReader &) = delete;

	AsyncFileReader &operator=(const AsyncFileReader &) = delete;

	// Enable moving
	AsyncFileReader(AsyncFileReader &&other) noexcept;

	AsyncFileReader &operator=(AsyncFileReader &&other) noexcept;

	~AsyncFileReader();

	void readAsync(long long offset = 0);

	std::string_view waitAndGetResult();
};

#endif
