#include "connection.h"
namespace drumlin {
    boost::asio::io_service io_service;

    unique_ptr<IOService> io_thread;
    void start_io(){
        if(!!io_thread)
            return;
        io_thread.reset(new IOService());
    }
    void stop_io(){
        io_thread->stop();
        io_thread.reset(nullptr);
    }
}
