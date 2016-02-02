#ifndef NETVEND_NV_EXCEPTION_H
#define NETVEND_NV_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <boost/shared_ptr.hpp>

#include "netvend/commands.h"

class NetvendCommandException : public std::runtime_error {
    boost::shared_ptr<commands::errors::Error> commandError_;
public:
    NetvendCommandException(commands::errors::Error* commandError);
    boost::shared_ptr<commands::errors::Error> commandError();
    virtual ~NetvendCommandException();
};

#endif