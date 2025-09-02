#ifdef __linux__

#include "AsyncFile/AsyncFileReader.hpp"
#include <utility>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

void AsyncFileReader::cleanup() {
	if (fd_ >= 0) {
		close(fd_);
	}
	if (buffer_) {
		free(buffer_);
	}
	io_uring_queue_exit(&ring_);
}

AsyncFileReader::AsyncFileReader(const std::string &path, const size_t buffer_size): buffer_size_(buffer_size) {
	// Open file
	fd_ = open(path.c_str(), O_RDONLY);
	if (fd_ < 0) {
		throw std::runtime_error("Failed to open file: " + path);
	}

	// Initialize io_uring
	if (io_uring_queue_init(8, &ring_, 0) < 0) {
		// Use a small queue size for simplicity (pros: less memory usage, cons: may not handle high load well)
		throw std::runtime_error("io_uring init failed");
	}

	// Allocate aligned buffer (4096 bytes is a common alignment for performance)
	buffer_ = static_cast<char *>(aligned_alloc(4096, buffer_size_));
	if (!buffer_) {
		throw std::runtime_error("Failed to allocate buffer");
	}
	memset(buffer_, 0, buffer_size_); // Initialize buffer to zero
}

AsyncFileReader::AsyncFileReader(AsyncFileReader &&other) noexcept: fd_(std::exchange(other.fd_, -1)),
                                                                    buffer_(std::exchange(other.buffer_, nullptr)),
                                                                    buffer_size_(other.buffer_size_),
                                                                    ring_(other.ring_) {
}

AsyncFileReader &AsyncFileReader::operator=(AsyncFileReader &&other) noexcept {
	if (this != &other) {
		cleanup();
		fd_ = std::exchange(other.fd_, -1);
		buffer_ = std::exchange(other.buffer_, nullptr);
		buffer_size_ = other.buffer_size_;
		ring_ = other.ring_;
	}
	return *this;
}

void AsyncFileReader::readAsync(const off_t offset) {
	// Start async read operation
	io_uring_sqe *sqe = io_uring_get_sqe(&ring_); // Get submission queue entry
	if (!sqe) {
		// Check if we got a valid submission queue entry
		throw std::runtime_error("Failed to get submission queue entry");
	}

	io_uring_prep_read(sqe, fd_, buffer_, buffer_size_, offset); // Prepare read operation
	io_uring_submit(&ring_); // Submit the request
}

std::string_view AsyncFileReader::waitAndGetResult() {
	// Wait for the read operation to complete and return the result
	io_uring_cqe *cqe; // Completion queue entry
	if (io_uring_wait_cqe(&ring_, &cqe) < 0) {
		// Wait for completion
		throw std::runtime_error("Failed to wait for completion");
	}

	if (cqe->res < 0) {
		// Check if the operation was successful
		io_uring_cqe_seen(&ring_, cqe); // Mark the completion as seen
		throw std::runtime_error("Async read failed: " + std::string(strerror(-cqe->res)));
	}

	std::string_view result(buffer_, cqe->res);
	io_uring_cqe_seen(&ring_, cqe); // Mark the completion as seen
	return result;
}

AsyncFileReader::~AsyncFileReader() {
	cleanup();
}

#elif _WIN32

#include "AsyncFile/AsyncFileReader.hpp"
#include <stdexcept>
#include <vector>

AsyncFileReader::AsyncFileReader(const std::string &path, size_t buffer_size)
	: buffer_size_(buffer_size) {
	hFile_ = CreateFile(
		path.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (hFile_ == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("Failed to open file for reading.");
	}

	buffer_ = new char[buffer_size_];
	overlapped_.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (overlapped_.hEvent == nullptr) {
        CloseHandle(hFile_);
        throw std::runtime_error("Failed to create event for async read.");
    }
}

AsyncFileReader::~AsyncFileReader() {
	cleanup();
}

void AsyncFileReader::cleanup() {
	if (overlapped_.hEvent) {
		CloseHandle(overlapped_.hEvent);
		overlapped_.hEvent = nullptr;
	}
	delete[] buffer_;
	buffer_ = nullptr;
	if (hFile_ != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile_);
		hFile_ = INVALID_HANDLE_VALUE;
	}
}

AsyncFileReader::AsyncFileReader(AsyncFileReader &&other) noexcept
	: hFile_(other.hFile_), buffer_(other.buffer_), buffer_size_(other.buffer_size_), overlapped_(other.overlapped_) {
	other.hFile_ = INVALID_HANDLE_VALUE;
	other.buffer_ = nullptr;
	other.overlapped_.hEvent = nullptr;
}

AsyncFileReader &AsyncFileReader::operator=(AsyncFileReader &&other) noexcept {
	if (this != &other) {
		cleanup();
		hFile_ = other.hFile_;
		buffer_ = other.buffer_;
		buffer_size_ = other.buffer_size_;
		overlapped_ = other.overlapped_;
		other.hFile_ = INVALID_HANDLE_VALUE;
		other.buffer_ = nullptr;
		other.overlapped_.hEvent = nullptr;
	}
	return *this;
}

void AsyncFileReader::readAsync(const long long offset) {
	overlapped_.Offset = offset & 0xFFFFFFFF;
	overlapped_.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;
	ResetEvent(overlapped_.hEvent);

	if (!ReadFile(hFile_, buffer_, static_cast<DWORD>(buffer_size_), nullptr, &overlapped_)) {
		if (GetLastError() != ERROR_IO_PENDING) {
			throw std::runtime_error("ReadFile failed.");
		}
	}
}

std::string_view AsyncFileReader::waitAndGetResult() {
	DWORD bytesRead;
	if (!GetOverlappedResult(hFile_, &overlapped_, &bytesRead, TRUE)) {
		throw std::runtime_error("GetOverlappedResult failed.");
	}
	return std::string_view(buffer_, bytesRead);
}

#endif
