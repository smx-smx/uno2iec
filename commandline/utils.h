// Utility functions for communication and data processing related to
// IEC bus and Arduino communication.

#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <string>

struct IECStatus {
  IECStatus() : status_code(OK) {}

  enum IECStatusCode {
    OK = 0x00,
    UNIMPLEMENTED = 0x01,
    CONNECTION_FAILURE = 0x02
  };
  IECStatusCode status_code;
  std::string message;  // A status message describing the status.
};

// Sets status according to the specified status_code and context, which will be
// used to populate the status message.
void SetError(IECStatus::IECStatusCode status_code, const std::string& context,
              IECStatus* status);

// Sets status according to the specified status_code and context, and appends a
// textual representation of the current errno.
void SetErrorFromErrno(IECStatus::IECStatusCode status_code,
                       const std::string& context, IECStatus* status);

// BufferedReadWriter can be used to read both terminated and fixed
// character amounts from a file handle. It buffers reads internally,
// writes are executed immediately. Note that file handle ownership is not
// transferred. The handle will not be closed upon destruction of a
// BufferedReadWriter.
class BufferedReadWriter {
 public:
  BufferedReadWriter(int fd) : fd_(fd) {}

  // Reads up to max_length characters until term_symbol is found and set result
  // to the read string (not including term_symbol). Returns true if successful,
  // sets status otherwise.
  bool ReadTerminatedString(char term_symbol, size_t max_length,
                            std::string* result, IECStatus* status);

  // Writes the specified content, with no terminator such as the null character
  // or newline. Returns true if successful, sets status otherwise.
  bool WriteString(const std::string& content, IECStatus* status);

 private:
  // File descriptor to read from.
  int fd_;
};

#endif  // UTILS_H
