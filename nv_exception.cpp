#include "nv_exception.h"

NetvendCommandException::NetvendCommandException(commands::errors::Error* commandError)
: runtime_error("Netvend agent command error.")
{
    commandError_ = boost::shared_ptr<commands::errors::Error>(commandError);
}

NetvendCommandException::~NetvendCommandException() throw() {}

boost::shared_ptr<commands::errors::Error> NetvendCommandException::commandError() {
    return commandError_;
}