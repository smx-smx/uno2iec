//
// bin_to_array.cc
//
// Reads binary data from stdin and produces a header file with
// a C array containing it on stdout.
//

#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "boost/format.hpp"

static const int kNumBytesPerLine = 13;

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cerr << "Syntax: bin_to_array <array_name> <input_binary> <output.h>"
              << std::endl;
    return 1;
  }
  int fd_in = open(argv[2], O_RDONLY);
  if (fd_in == -1) {
    std::cerr << "Error opening '" << argv[2]
              << "' for reading: " << strerror(errno) << std::endl;
    return 1;
  }
  std::ofstream output;
  output.open(argv[3], std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    // Some other error occurred.
    std::cerr << "Error opening '" << argv[3]
              << "' for writing: " << strerror(errno) << std::endl;
    return 1;
  }

  char buffer[kNumBytesPerLine];
  size_t bytes_left = kNumBytesPerLine;
  output << "static const unsigned char " << argv[1] << "[] = {" << std::endl;
  while (true) {
    ssize_t read_result =
        read(fd_in, buffer + kNumBytesPerLine - bytes_left, bytes_left);
    if (read_result == -1) {
      if (read_result == EAGAIN || read_result == EWOULDBLOCK) {
        // Try again.
        continue;
      }
      // Some other error occurred.
      std::cerr << "Error reading from stdin: " << strerror(errno) << std::endl;
      return 1;
    }
    bytes_left -= read_result;
    if (bytes_left == 0 || read_result == 0) {
      size_t bytes_to_render = kNumBytesPerLine - bytes_left;
      output << "  ";
      for (unsigned int i = 0; i < bytes_to_render; ++i) {
        output << (boost::format("0x%02x") %
                   static_cast<unsigned int>(
                       static_cast<unsigned char>(buffer[i])))
                      .str();
        if (i < bytes_to_render - 1) {
          output << ", ";
        } else if (read_result > 0) {
	  // Comma at the end of line, not the last line.
	  output << ",";
	}
      }
      output << std::endl;
      if (read_result == 0)
        break; // The file is done.
      bytes_left = kNumBytesPerLine;
    }
  }
  output << "};" << std::endl;
  output.close();
  close(fd_in);

  return 0;
}
