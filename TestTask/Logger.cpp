#include "Logger.h"

Logger::Logger() {
    instanceId = std::to_string(getpid());
}

void Logger::log(const std::string& message) {

    std::stringstream formattedMessage;
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    formattedMessage << std::ctime(&now_c) << " - " << message;

    std::string filename = "log_instance_" + instanceId + ".txt";
    std::ofstream logfile(filename, std::ios_base::app);
    if (logfile.is_open())
    {
        _lock.lock();
        logfile << formattedMessage.str() << std::endl;
        logfile.close();
        // Отпускаем мьютекс после записи в лог
        _lock.unlock();
    } else {
        std::cerr << "Unable to open log file: " << filename << std::endl;
    }
}

void Logger::logError(const std::exception& e) {
    std::stringstream errorMessage;
    errorMessage << "Error: " << e.what();

    std::string filename = "log_instance_" + instanceId + ".txt";
    std::ofstream logfile(filename, std::ios_base::app);
    if (logfile.is_open()) {
        _lock.lock();
        // Критическая секция, где мьютекс заблокирован
        logfile << errorMessage.str() << std::endl;

        // Отпускаем мьютекс после записи в лог
        _lock.unlock();
    } else {
        std::cerr << "Unable to open log file!" << std::endl;
    }
}
