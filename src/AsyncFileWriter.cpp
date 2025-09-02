#ifdef __linux__

#include "AsyncFile/AsyncFileWriter.hpp"

#include <utility>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

void AsyncFileWriter::cleanup() {
	if (fd_ >= 0) close(fd_);
	io_uring_queue_exit(&ring_);
}

AsyncFileWriter::AsyncFileWriter(const std::string &path, const bool append) {
	int flags = O_WRONLY | O_CREAT;
	flags |= append ? O_APPEND : O_TRUNC;
	fd_ = open(path.c_str(), flags, 0644);
	if (fd_ < 0) throw std::runtime_error("Failed to open file for writing: " + path);
	if (io_uring_queue_init(8, &ring_, 0) < 0) throw std::runtime_error("io_uring init failed");
}

AsyncFileWriter::AsyncFileWriter(AsyncFileWriter &&other) noexcept
	: fd_(std::exchange(other.fd_, -1)),
	  buffer_(std::exchange(other.buffer_, nullptr)),
	  buffer_size_(other.buffer_size_),
	  ring_(other.ring_) {
}

AsyncFileWriter &AsyncFileWriter::operator=(AsyncFileWriter &&other) noexcept {
	if (this != &other) {
		cleanup();
		fd_ = std::exchange(other.fd_, -1);
		buffer_ = std::exchange(other.buffer_, nullptr);
		buffer_size_ = other.buffer_size_;
		ring_ = other.ring_;
	}
	return *this;
}


void AsyncFileWriter::wait() {
	io_uring_cqe *cqe;
	if (io_uring_wait_cqe(&ring_, &cqe) < 0) {
		throw std::runtime_error("Failed to wait for write completion");
	}

	if (cqe->res < 0) {
		io_uring_cqe_seen(&ring_, cqe);
		throw std::runtime_error("Async write failed: " + std::string(strerror(-cqe->res)));
	}

	io_uring_cqe_seen(&ring_, cqe);
}

AsyncFileWriter::~AsyncFileWriter() {
	cleanup();
}

void AsyncFileWriter::writeAsync(const char *data, const size_t buffer_size, const off_t offset) {
	buffer_ = data;
	buffer_size_ = buffer_size;
	io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
	if (!sqe) throw std::runtime_error("Failed to get submission queue entry");
	io_uring_prep_write(sqe, fd_, buffer_, buffer_size_, offset);
	io_uring_submit(&ring_);
}

#elif _WIN32

#include "AsyncFile/AsyncFileWriter.hpp"
#include <stdexcept>

AsyncFileWriter::AsyncFileWriter(const std::string &path, const bool append) {
	hFile_ = CreateFile(
		path.c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		append ? OPEN_ALWAYS : CREATE_ALWAYS,
		FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (hFile_ == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("Failed to open file for writing.");
	}
	overlapped_.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (overlapped_.hEvent == nullptr) {
        CloseHandle(hFile_);
        throw std::runtime_error("Failed to create event for async write.");
    }
}

AsyncFileWriter::~AsyncFileWriter() {
	cleanup();
}

void AsyncFileWriter::cleanup() {
	if (overlapped_.hEvent) {
		CloseHandle(overlapped_.hEvent);
		overlapped_.hEvent = nullptr;
	}
	if (hFile_ != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile_);
		hFile_ = INVALID_HANDLE_VALUE;
	}
}

AsyncFileWriter::AsyncFileWriter(AsyncFileWriter &&other) noexcept
	: hFile_(other.hFile_), buffer_(other.buffer_), buffer_size_(other.buffer_size_), overlapped_(other.overlapped_) {
	other.hFile_ = INVALID_HANDLE_VALUE;
	other.buffer_ = nullptr;
	other.overlapped_.hEvent = nullptr;
}

AsyncFileWriter &AsyncFileWriter::operator=(AsyncFileWriter &&other) noexcept {
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

void AsyncFileWriter::writeAsync(const char *data, const size_t buffer_size, const long long offset) {
	buffer_ = data;
	buffer_size_ = buffer_size;
	overlapped_.Offset = offset & 0xFFFFFFFF;
	overlapped_.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;
	ResetEvent(overlapped_.hEvent);

	if (!WriteFile(hFile_, buffer_, buffer_size_, nullptr, &overlapped_)) {
		if (GetLastError() != ERROR_IO_PENDING) {
			throw std::runtime_error("WriteFile failed.");
		}
	}
}

void AsyncFileWriter::wait() {
	DWORD bytesWritten;
	if (!GetOverlappedResult(hFile_, &overlapped_, &bytesWritten, TRUE)) {
		throw std::runtime_error("GetOverlappedResult failed.");
	}
}

#endif
