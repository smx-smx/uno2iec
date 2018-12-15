#include <errno.h>
#include <string.h>

#include "utils.h"

void SetError(IECStatus::IECStatusCode status_code,
	      const std::string& context, IECStatus* status) {
  status->status_code = status_code;
  switch (status_code) {
    case IECStatus::OK:
      status->message = "OK";
      break;
    case IECStatus::UNIMPLEMENTED:
      status->message = "Unimplemented";
      break;
    case IECStatus::CONNECTION_FAILURE:
      status->message = "Connection failure";
      break;
  }
  if (!context.empty()) {
    status->message = context + ": " + status->message;
  }
}

void SetErrorFromErrno(IECStatus::IECStatusCode status_code,
                       const std::string& context, IECStatus* status) {
  SetError(status_code, context, status);
  status->message = status->message + ": " + strerror(errno);
}

bool ReadTerminatedString(int fd, char term_symbol, size_t max_length,
                          std::string* result, IECStatus* status) {
  return false;
}
