#pragma once
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

#define OCCLUDE_SOCKET_RELATIVE_PATH "/occlude.sock"
#define OCCLUDE_SOCKET_FALLBACK_ABSOLUTE_PATH "/tmp/occlude.sock"

namespace utilities {
  std::string strFromMessage(std::string_view message) {
    return std::string(message).append("\n");
  }
  std::string socketPath(std::string const& xdgPath) {
    std::string result = xdgPath;
    result.append(OCCLUDE_SOCKET_RELATIVE_PATH);
    return result;
  }
} // namespace utilities

namespace IPC {
  enum class Error : std::uint8_t {
    Accept,         //
    Bind,           //
    Connect,        //
    Listen,         //
    Read,           //
    SocketCreation, //
    Write           //
  };

  struct Connection {
    int fileDescriptor = -1;

    Connection() = default;
    explicit Connection(int fileDescriptorV) : fileDescriptor(fileDescriptorV) {}
    ~Connection() {
      const bool fileDescriptorValid = fileDescriptor != -1;
      if(fileDescriptorValid) {
        close(fileDescriptor);
      }
    }

    // compliance with special member functions rule
    // no copy semantics, only move
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept : fileDescriptor(other.fileDescriptor) {
      other.fileDescriptor = -1;
    }
    Connection& operator=(Connection&& other) noexcept {
      const bool isSelfAssignment = this == &other;
      if(!isSelfAssignment) {
        const bool fileDescriptorValid = fileDescriptor != -1;
        if(fileDescriptorValid) {
          close(fileDescriptor);
        }
        fileDescriptor = other.fileDescriptor;
        other.fileDescriptor = -1;
      }
      return *this;
    }

    [[nodiscard]] std::expected<void, Error> send(std::string_view message) const {
      std::string packet = utilities::strFromMessage(message);
      ssize_t bytes = ::write(fileDescriptor, packet.data(), packet.size());
      const bool writeFailed = bytes < 0;
      if(writeFailed) {
        return std::unexpected(Error::Write);
      }
      return {};
    }

    [[nodiscard]] std::expected<std::string, Error> receive() const {
      std::string buffer;
      std::array<char, 1024> chunk{};
      bool sawNewline = false;

      while(!sawNewline) {
        ssize_t bytes = ::read(fileDescriptor, chunk.data(), chunk.size());
        if(bytes < 0) {
          if(errno == EINTR) {
            continue; 
          }
          return std::unexpected(Error::Read);
        }
        if(bytes == 0) {
          break; 
        }

        const auto* start = chunk.data();
        const auto* end = start + bytes;
        const auto* newlinePos = std::find(start, end, '\n');

        if(newlinePos != end) {
          buffer.append(start, static_cast<std::size_t>(newlinePos - start));
          sawNewline = true;
          break;
        }
        buffer.append(start, static_cast<std::size_t>(end - start));
      }

      if(!sawNewline) {
        return std::unexpected(Error::Read);
      }
      return buffer;
    }
  };

  inline std::string getSocketPath() {
    const char* const xdg = std::getenv("XDG_RUNTIME_DIR");
    const bool hasXdgRuntimeDir = xdg != nullptr;
    if(hasXdgRuntimeDir) {
      return utilities::socketPath(xdg);
    }
    return OCCLUDE_SOCKET_FALLBACK_ABSOLUTE_PATH;
  }

  struct Server {
    int fileDescriptor = -1;
    std::string path;

    Server() = default;

    // compliance with special member functions rule
    // no copy semantics, only move
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(Server&& other) noexcept : fileDescriptor(other.fileDescriptor), path(std::move(other.path)) {
      other.fileDescriptor = -1;
    }

    Server& operator=(Server&& other) noexcept {
      const bool selfAssignment = this == &other;
      if(!selfAssignment) {
        const bool descriptorValid = fileDescriptor != -1;
        if(descriptorValid) {
          close(fileDescriptor);
          const auto* const cPath = path.c_str();
          ::unlink(cPath);
        }
        fileDescriptor = other.fileDescriptor;
        path = std::move(other.path);
        other.fileDescriptor = -1;
      }
      return *this;
    }

    [[nodiscard]] static std::expected<Server, Error> create() {
      Server server;

      server.path = getSocketPath();
      server.fileDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
      const bool creationFailed = server.fileDescriptor == -1;
      if(creationFailed) {
        return std::unexpected(Error::SocketCreation);
      }

      const auto* const cServerPath = server.path.c_str();
      ::unlink(cServerPath);

      sockaddr_un address{};
      address.sun_family = AF_UNIX;
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
      std::strncpy(address.sun_path, cServerPath, sizeof(address.sun_path) - 1);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      auto* const socketAddress = reinterpret_cast<sockaddr*>(&address);
      const std::size_t ptrSize = sizeof(address);
      const auto bindResult = ::bind(server.fileDescriptor, socketAddress, ptrSize);

      const bool bindFailed = bindResult == -1;
      if(bindFailed) {
        close(server.fileDescriptor);
        return std::unexpected(Error::Bind);
      }

      const auto listenResult = ::listen(server.fileDescriptor, 5);

      const bool listenFailed = listenResult == -1;
      if(listenFailed) {
        close(server.fileDescriptor);
        return std::unexpected(Error::Listen);
      }

      return server;
    }

    [[nodiscard]] std::expected<Connection, Error> accept() const {
      int clientFileDescriptor = ::accept(fileDescriptor, nullptr, nullptr);
      const bool acceptFailed = clientFileDescriptor == -1;
      if(acceptFailed) {
        return std::unexpected(Error::Accept);
      }
      return Connection{clientFileDescriptor};
    }

    ~Server() {
      const bool fileDescriptorValid = fileDescriptor != -1;
      if(fileDescriptorValid) {
        close(fileDescriptor);
        const auto* const cPath = path.c_str();
        ::unlink(cPath);
      }
    }
  };

  [[nodiscard]] inline std::expected<Connection, Error> connect() {
    int fileDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
    const bool socketCreationFailed = fileDescriptor == -1;
    if(socketCreationFailed) {
      return std::unexpected(Error::SocketCreation);
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::string path = getSocketPath();
    const auto* const cPath = path.c_str();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::strncpy(address.sun_path, cPath, sizeof(address.sun_path) - 1);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* const socketAddress = reinterpret_cast<sockaddr*>(&address);
    const std::size_t ptrSize = sizeof(address);
    const auto connectResult = ::connect(fileDescriptor, socketAddress, ptrSize);
    const bool connectFailed = connectResult == -1;
    if(connectFailed) {
      close(fileDescriptor);
      return std::unexpected(Error::Connect);
    }
    return Connection{fileDescriptor};
  }
} // namespace IPC
