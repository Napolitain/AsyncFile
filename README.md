# AsyncFile

### Goal

- support async file IO in Linux, Windows, and MacOS using
    - io_uring for Linux
    - IOCP for Windows
    - tbd for MacOS
- support async file IO in C++20 coroutines
- common interface to use easily with code supposed to be blocking (std::fstream, etc.)

