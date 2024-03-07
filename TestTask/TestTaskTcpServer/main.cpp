#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <map>
#include <sstream>
#include <thread>
#include <vector>
#include <numeric>

#include "../ClientTask.h"
#include "../Logger.h"

std::stringstream ss;


using namespace boost::archive;
using boost::asio::ip::tcp;

std::ostream& flush(std::ostream& os)
{
    os << std::flush;
    return os;
}

Logger logger;

class IntegrationParams {
public:
    static double lowerBound;
    static double upperBound;
    static double step;
    static int typeIntegration; //0 - метод прямоугольников; 1 - метод Монте-Карло

    static void printParams()
    {
        std::cout << "Lower Bound: " << lowerBound << std::endl;
        std::cout << "Upper Bound: " << upperBound << std::endl;
        std::cout << "Step: " << step << std::endl;
        if(typeIntegration == 1) {std::cout << "typeIntegration: Monte-Carlo" << std::endl;}
        if(typeIntegration == 0) {std::cout << "typeIntegration: Rectangle method" << std::endl;}
    }
};

class Session
        : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    tcp::socket& getSocket() { return socket_; }

    void start() //инициирует чтение данных из сокета клиента.
    {
        doReadNumThreadsClient();
    }

    int getUserThreads() const { return clientThreads; }

    void setClientTask(const ClientTask& clientTask) {
        this->clientTask_ = clientTask;
    }

    void setLocResult(const double value) {
        locResult = value;
    }

    double getLocResult() const {
        return locResult;
    }

    void doWriteTask(const ClientTask& task)
    {
        clientTask_ = task;
        auto self(shared_from_this());

        if (socket_.is_open()) {
            std::cout<< "Socket's open :"<< reinterpret_cast<void*>(&socket_) << std::endl;
            logger.log("\nSocket's open\n");

            text_oarchive oa{ss};
            oa << clientTask_;
            std::string serializedDataForClient = ss.str() ;
            size_t dataSize = serializedDataForClient.size();

            std::cout << serializedDataForClient << std::endl;
            std::cout << serializedDataForClient.length() << std::endl;
            logger.log(serializedDataForClient);

            boost::asio::async_write(socket_, boost::asio::buffer(serializedDataForClient),
                                     [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::cout << "Recording success" << std::endl;
                    logger.log("Recording success");
                    doReadResultFromClient();
                } else {
                    // Обработка ошибки
                    std::cout << "Record error handling" << std::endl;
                    logger.log("Record error handling");
                }
            });
            ss.str(std::string());
            serializedDataForClient.clear();

        } else {
            std::cout<< "Socket's closed" << std::endl;
            logger.log("\nSocket's closed\n");
        }

    }

    void setResultCallback(std::function<void(double)> callback) {
        resultCallback_ = std::move(callback);
    }

    ClientTask clientTask_;
    bool worker = false;
    bool resReady = false;
    
private:
    void doReadNumThreadsClient() //асинхронно читает данные из сокета
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                std::cout << "::New connect:: threads client::" << std::string{data_, length} << std::endl;
                logger.log("::New connect:: threads client::" + std::string{data_, length});
                // Проверка корректности данных
                std::string receivedData(data_, length);

                try {
                    int lThreads = std::stoi(receivedData); // Попытаться преобразовать принятые данные в число
                    clientThreads = lThreads;
                } catch (const std::invalid_argument& e) {
                    // Проблема с преобразованием в число, обработать ошибку
                    std::cerr << "Error: Unable to convert received data to a number" << e.what() << std::endl;
                    logger.logError(e);
                    clientThreads = 1;
                }
            }
        });
    }
    void doReadResultFromClient() //асинхронно читает данные из сокета
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                std::cout << "\n::doReadResultFromClient::" << length << "=" << std::string{data_, length} << std::endl;
                // Проверка корректности данных
                std::string receivedData(data_, length);

                try {
                    double lResult = std::stod(receivedData); // Попытаться преобразовать принятые данные в число
                    locResult = lResult;
                    std::string strLog = "locRes:" + std::to_string(locResult);
                    logger.log(strLog);
                    // Вызов функции обратного вызова
                    if (resultCallback_) {
                        std::cout << "::resultCallback_::\n";
                        resultCallback_(lResult);
                    }
                } catch (const std::invalid_argument& e) {
                    locResult = -1;
                    std::cerr << "Error: Unable to convert received data to a number" << e.what() << std::endl;
                }
            }
        });
    }

    std::function<void(double)> resultCallback_;
    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
    int clientThreads = -1;
    double locResult = -1;
};


class ServerTcp
{
public:
    ServerTcp(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context_(io_context)
    {
        doAccept();
    }

    void startHandlingInput()
    {
        std::string input;
        while (true)
        {
            std::cin >> input;
            if (input == "/start")
            {
                std::cout << "Comand /start get!\n";
                logger.log("Comand /start get!\n");
                sendingTasks();
            }
            if (input == "/status")
            {
                std::cout << "Comand /status get!\n";
                logger.log("Comand /status get!\n");
                printStatusSession();
            }
        }
    }

private:
    void doAccept()
    {
        acceptor_.async_accept(
                    [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::shared_ptr<Session> session = std::make_shared<Session>(std::move(socket));

                session->setResultCallback([this, session](double result) {
                    
                    std::cout << "::getLocResult(): " <<  session->getLocResult();
                    vecClientResults.push_back(session->getLocResult());
                    std::cout << vecClientResults.size() << std::endl;
                    if (checkIfAllClientsFinishedComputing())
                    {
                        flagFinished = true;
                        resultCalculations = std::accumulate(vecClientResults.begin(), vecClientResults.end(), 0.0);
                        std::cout << "\n::Calculation result::" << resultCalculations << flush << std::endl;
                        logger.log("::Calculation result::" + std::to_string(resultCalculations));
                    }
                });

                sessions_[nextSessionId_++] = session;
                
                session->start();
            }

            doAccept();
        });
    }
    bool checkIfAllClientsFinishedComputing()
    {
        bool flag{true};
        int countWorkerClient = 0;

        for (const auto& pair : sessions_)
        {
            const auto& sessionId = pair.first;
            const auto& session = pair.second;
            if(session->worker)
            {
                countWorkerClient++;
            }
        }
        if(countWorkerClient == vecClientResults.size()) { return flag; }

        return false;
    }
    void sendingTasks()
    {
        int numAllClientThreads = 0;
        int numConnectedUsers = sessions_.size();
        int numSteps = static_cast<int>((IntegrationParams::upperBound - IntegrationParams::lowerBound)
                                        / IntegrationParams::step);

        double locLowerBound = IntegrationParams::lowerBound; //границы интегрирования
        double locUpperBound = IntegrationParams::lowerBound;

        for (const auto& pair : sessions_)
        {
            const auto& sessionId = pair.first;
            const auto& session = pair.second;
            numAllClientThreads += session->getUserThreads();
        }

        int numStepsOneThread = numSteps / numAllClientThreads;

        for (const auto& pair : sessions_)
        {
            const auto& sessionId = pair.first;
            const auto& session = pair.second;

            if ((sessionId + 1) < sessions_.size())
            {
                locLowerBound = (sessionId == 0) ? locLowerBound : locUpperBound;
                locUpperBound = locLowerBound + (session->getUserThreads() * numStepsOneThread * IntegrationParams::step);
            }else
            {
                locLowerBound = locUpperBound;
                locUpperBound = IntegrationParams::upperBound;
            }
            // Отправляем задание клиенту
            ClientTask clientTask_{locLowerBound, locUpperBound, IntegrationParams::step, session->getUserThreads(), IntegrationParams::typeIntegration};
            session->worker = true;

            session->doWriteTask(clientTask_);
            std::cout<<" Session id: " << sessionId << "\n client threads:" << session->getUserThreads() <<
                       " - lb = " << locLowerBound << " - ub = " << locUpperBound <<std::endl;
            std::string strLog("\nSession id:" + std::to_string(sessionId));
            logger.log(strLog);
        }
        std::cout << "Number of total connected customers: " << sessions_.size() << std::endl;
        std::string strLog("\nNumber of total connected customers:" + std::to_string(sessions_.size()));
        logger.log(strLog);
    }
    void printStatusSession()
    {
        for (const auto& pair : sessions_)
        {
            const auto& sessionId = pair.first;
            const auto& session = pair.second;

            bool clientWorker = session->worker;
            std::string strClientWorker = (clientWorker) ? "true" : "false";
            bool clientResReady = session->resReady;
            std::string strResReady = (clientResReady) ? "true" : "false";

            
            std::cout<<" Session id: " << sessionId << " - Client worker ? " << strClientWorker
                    << "\n client threads:" << session->getUserThreads() <<std::endl;

        }

        std::cout << "Number of total connected customers: " << sessions_.size() << std::endl;
        if(flagFinished) { std::cout << "Calculation result:" << resultCalculations; }

    }


    std::map<int, std::shared_ptr<Session>> sessions_;
    int nextSessionId_ = 0;
    bool flagFinished = false;
    double resultCalculations = -1;
    tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    std::shared_ptr<boost::asio::steady_timer> timer_;

    std::vector<double> vecClientResults;
};


double IntegrationParams::lowerBound = 3.0; //Default value --> Integral(a:b)(f(x)) ~ 2.0005
double IntegrationParams::upperBound = 5.8956;
double IntegrationParams::step = 0.0001;
int    IntegrationParams::typeIntegration = 1;

bool checkCorrectArgv() 
{
    bool flag = true;

    if (IntegrationParams::upperBound < 0 ) flag = false;
    if (IntegrationParams::lowerBound < 0 ) flag = false;
    if ((IntegrationParams::lowerBound <= 1) && (IntegrationParams::upperBound >= 1)) flag = false;
    if (IntegrationParams::step <= 0 ) flag = false;
    if ((IntegrationParams::upperBound - IntegrationParams::lowerBound) <=0)  flag = false;
    if ((IntegrationParams::typeIntegration != 0 && IntegrationParams::typeIntegration != 1)) flag = false;

    return flag;
}

int main(int argc, char* argv[])
{
    std::string commandLineStr= "";
    for (int i = 1;i < argc; i++) commandLineStr.append(std::string(argv[i]).append(" "));

    const short port{3456};
    try
    {
        if (argc < 5) {
            std::cout << "WARNING! Using default integration parameters\n";
            logger.log("WARNING! Using default integration parameters\n");
        }
        else
        {
            // Считываем значения из аргументов командной строки
            try
            {
                IntegrationParams::lowerBound = std::stod(argv[1]);
                IntegrationParams::upperBound = std::stod(argv[2]);
                IntegrationParams::step = std::stod(argv[3]);
                IntegrationParams::typeIntegration = std::stoi(argv[4]);

                logger.log("Params exec:" + commandLineStr);
                if(!checkCorrectArgv())
                {
                    logger.log("WARNING! Uncorrect params; f(x)= 1/ln(x), => x > 0, ln(x) != 0, b > a;\n");
                    std::cout << "WARNING! Uncorrect params; f(x)= 1/ln(x), => x > 0, ln(x) != 0, b > a;\n";
                    return 1;
                }


            } catch(const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                logger.logError(e);
                return 1;
            }
        }
        IntegrationParams::printParams();
        boost::asio::io_context io_context;
        ServerTcp testServer(io_context, port);

        std::thread inputThread([&testServer]() {
            testServer.startHandlingInput();
        });

        io_context.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    return 0;
}
