#pragma once

#ifdef __linux__

#include <string>
#include <liburing.h>

class AsyncFileWriter {
	int fd_ = -1;
	const char *buffer_ = nullptr;
	size_t buffer_size_ = 0;
	io_uring ring_;

	void cleanup();

public:
	explicit AsyncFileWriter(const std::string &path, bool append = false);

	AsyncFileWriter(const AsyncFileWriter &) = delete;

	AsyncFileWriter &operator=(const AsyncFileWriter &) = delete;

	AsyncFileWriter(AsyncFileWriter &&other) noexcept;

	AsyncFileWriter &operator=(AsyncFileWriter &&other) noexcept;

	~AsyncFileWriter();

	void writeAsync(const char *data, size_t buffer_size, off_t offset = 0);

	void wait();
};

#elif _WIN32

#include <string>
#include <windows.h>

class AsyncFileWriter {
	HANDLE hFile_ = INVALID_HANDLE_VALUE;
	const char *buffer_ = nullptr;
	size_t buffer_size_ = 0;
	OVERLAPPED overlapped_{};

	void cleanup();

public:
	explicit AsyncFileWriter(const std::string &path, bool append = false);

	// Disable copying
	AsyncFileWriter(const AsyncFileWriter &) = delete;

	AsyncFileWriter &operator=(const AsyncFileWriter &) = delete;

	// Enable moving
	AsyncFileWriter(AsyncFileWriter &&other) noexcept;

	AsyncFileWriter &operator=(AsyncFileWriter &&other) noexcept;

	~AsyncFileWriter();

	void writeAsync(const char *data, size_t buffer_size, long long offset = 0);

	void wait();
};

#endif
