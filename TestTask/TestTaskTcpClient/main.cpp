#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <chrono>
#include <thread>
#include <cmath>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <random>

#include "../ClientTask.h"
#include "../Logger.h"

using namespace boost::archive;

enum { max_length = 1024 };
std::stringstream ss;

Logger logger;

double func(double x) {
    return 1 / std::log(x);
}
//метод прямоугольников
double integrate(double a, double b, int n) {
    double dx = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        double x = a + i * dx;
        sum += func(x) * dx;
    }
    return sum;
}
double monteCarloIntegration(double a, double b, int n) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(a, b);

    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        double x = dis(gen);
        sum += func(x);
    }
    return (b - a) * sum / n;
}


double performIntegration(const ClientTask& task_, int useThreadsCount) 
{
        std::vector<double> partial_results(useThreadsCount);
        std::vector<std::thread> threads;
            double a = task_.getLowerBound();
            double b = task_.getUpperBound();
            int num_partitions = (b - a) / task_.getStep();
            std::cout << num_partitions << '\n';
            //Вычисление интеграла
            for (int i = 0; i < useThreadsCount; ++i) {
                threads.emplace_back([i, useThreadsCount, &partial_results, task_, a, b, num_partitions]() {
                    int chunk_size = num_partitions / useThreadsCount;
                    int start = i * chunk_size;
                    int end = (i == useThreadsCount - 1) ? num_partitions : start + chunk_size;
                    if(task_.getTypeUseMethods() == 0) 
                    {
                        partial_results[i] = integrate(a + start * (b - a) / num_partitions, a + end * (b - a) / num_partitions, end - start);
                    }
                    if(task_.getTypeUseMethods() == 1) 
                    {
                        partial_results[i] = monteCarloIntegration(a + start * (b - a) / num_partitions, a + end * (b - a) / num_partitions, end - start);
                    }
                });
            }

            for (auto& t : threads) {
                t.join();
            }

            double result = 0.0;
            for (const auto& res : partial_results) {
                result += res;
            }
    return result;
}

int main(int argc, char* argv[])
{
    boost::asio::streambuf receive_buffer;
    const unsigned int numThreadsMaxUse = (std::thread::hardware_concurrency() > 0) ?
                std::thread::hardware_concurrency() : 1;

    using boost::asio::ip::tcp;
    try
    {
        if (argc < 2) {
            std::cout << "Count using threads set default\n";
            logger.log("Count using threads set default\n");
            return 1;
        }

        int useThreadsCount = std::stoi(argv[1]);
        if (useThreadsCount > numThreadsMaxUse) {
            useThreadsCount = numThreadsMaxUse;
            std::cout << "Warning! Set uncorrent counts threads. Count using threads set default\n";
            logger.log("Warning! Set uncorrent counts threads. Count using threads set default\n");
        }

        boost::asio::io_context io_context;

        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve("127.0.0.1", "3456"));
        std::string sClientIp = socket.remote_endpoint().address().to_string();
        unsigned short uiClientPort = socket.remote_endpoint().port();
        logger.log(sClientIp + std::to_string(uiClientPort));

        boost::asio::streambuf receiveBuffer;
        boost::system::error_code ec;
        
        std::string combined_message = std::to_string(useThreadsCount);
        auto write_result = boost::asio::write(socket, boost::asio::buffer(combined_message)); // отправка на сервер кол-ва потоков

        logger.log("Numbers sendt::" + combined_message);
        std::cout << " Numbers sendt:: " << useThreadsCount << std::endl;

        boost::asio::read(socket, receive_buffer, boost::asio::transfer_at_least(1), ec); // чтение сериализованных данных с сервера

        if( ec && ec != boost::asio::error::eof ) {
            std::cout << "receive failed: " << ec.message() << std::endl;
        }
        else {
            std::string data_to_process(boost::asio::buffers_begin(receive_buffer.data()),
                                        boost::asio::buffers_begin(receive_buffer.data()) + receive_buffer.size());
            std::cout << "data_to_process:" << data_to_process << std::endl;
            ss << data_to_process;
            logger.log("data_to_process::" + data_to_process);
            //std::this_thread::sleep_for(std::chrono::seconds(useThreadsCount));
            text_iarchive ia{ss};
            ClientTask clientTask_;
            ia >> clientTask_;
            double result = performIntegration(clientTask_, useThreadsCount);

            std::cout << "Integral: " << result << std::endl;
            logger.log("Integral: " + std::to_string(result));

            double locResult = result;
            std::string combined_message = std::to_string(locResult);
            auto write_result = boost::asio::write(socket, boost::asio::buffer(combined_message)); //отправка данных на сервер

            std::cout << "locResult:: " << locResult << std::endl;
        }
        logger.log("socket.shutdown::");
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket.close();

    }catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        logger.logError(e);
    }
    return 0;
}
