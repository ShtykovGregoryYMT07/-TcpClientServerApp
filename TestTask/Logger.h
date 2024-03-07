#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <unistd.h> // Для getpid
#include <mutex>

class Logger {
public:
    Logger();
    void log(const std::string& message);
    void logError(const std::exception& e);

private:
    std::string instanceId;
    std::recursive_mutex _lock;
};

#endif // LOGGER_H