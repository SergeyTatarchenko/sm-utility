/**
 * @file sm_client.hpp
 *
 * @brief
 *
 * @author Siarhei Tatarchanka
 *
 */

#ifndef SM_CLIENT_H
#define SM_CLIENT_H

#include <atomic>
#include <future>
#include <memory>
#include <queue>
#include <thread>

#include "../../external/simple-serial-port-1.03/lib/inc/serial_port.hpp"
#include "../inc/sm_error.hpp"
#include "../inc/sm_file.hpp"
#include "../inc/sm_modbus.hpp"

namespace sm
{
//////////////////////////////SERVER CONSTANTS//////////////////////////////////
constexpr int boot_version_size = 17;
constexpr int boot_name_size = 33;
constexpr int amount_of_regs = 7;
constexpr int not_connected = -1;
constexpr std::uint16_t file_read_prepare = 1;
constexpr std::uint16_t file_write_prepare = 2;
constexpr std::uint16_t app_erase_request = 1;
constexpr std::uint16_t app_start_request = 1;
////////////////////////////////////////////////////////////////////////////////

enum class ServerRegisters
{
    file_control = 0,
    app_size = 1,
    app_erase = 2,
    app_start = 3,
    boot_control = 4,
    boot_status = 5,
    record_size = 6
};

enum class BootloaderStatus
{
    unknown = 0,
    empty = 1,
    ready = 2,
    error = 3
};

enum class ServerFiles
{
    application = 1,
    server_metadata = 2
};

enum class ClientTasks
{
    undefined,
    regs_read,
    reg_write,
    file_read,
    file_write,
    ping,//extra command, FunctionCodes::undefined used
    app_start,//extra command, the same as reg_write, but no responce expected
    info_download,
    app_upload,
    app_download,
};

struct TaskAttributes
{
    TaskAttributes() = default;
    TaskAttributes(modbus::FunctionCodes code, size_t length)
        : code(code), length(length)
    {
    }
    modbus::FunctionCodes code = modbus::FunctionCodes::undefined;
    size_t length = 0;
};

struct TaskInfo
{
    TaskInfo() = default;
    TaskInfo(ClientTasks task, int num_of_exchanges)
        : task(task), num_of_exchanges(num_of_exchanges){};
    ClientTasks task = ClientTasks::undefined;
    TaskAttributes attributes;
    std::error_code error_code;
    int num_of_exchanges = 0;
    int counter = 0;
    std::atomic<bool> done = false;
    void reset(ClientTasks task = ClientTasks::undefined, int num_of_exchanges = 0)
    {
        this->task = task;
        this->num_of_exchanges = num_of_exchanges;
        counter = 0;
        done = false;
        attributes = TaskAttributes();
        error_code = std::error_code();
    }
};

#pragma pack(push)
#pragma pack(2)
struct BootloaderInfo
{
    char boot_version[boot_version_size];
    char boot_name[boot_name_size];
    std::uint32_t available_rom;
};
#pragma pack(pop)

enum class ServerStatus
{
    Unavailable,
    Available
};

struct ServerInfo
{
    std::uint8_t addr = 0;
    ServerStatus status = ServerStatus::Unavailable;
};

struct ServerData
{
    ServerInfo info;
    std::uint16_t regs[amount_of_regs] = {};
    BootloaderInfo data = {};
};

class Client
{
public:
    Client() : client_thread(&Client::clientThread, this) {}
    ~Client();
    /// @brief start client
    /// @param device device name to use
    /// @return error code
    std::error_code start(std::string device);
    /// @brief stop client, close port
    void stop();
    /// @brief client device configure
    /// @param config used config
    /// @return error code
    std::error_code configure(sp::PortConfig config);
    /// @brief connect to server with selected id
    /// @param address server address
    /// @return error code
    std::error_code connect(const std::uint8_t address);
    /// @brief erase firmware on the server
    /// @return error code
    std::error_code eraseApp();
    /// @brief upload new firmware
    /// @param path_to_file path to file
    /// @return error code
    std::error_code uploadApp(const std::string path_to_file);
    /// @brief start application
    /// @return error code
    std::error_code startApp();
    /// @brief diconnect from server
    void disconnect();
    /// @brief load last received server data
    /// @param data reference to struct to save
    void getServerData(ServerData& data);
    /// @brief get actual running task progress in %
    /// @return value from 0 to 100
    int getActualTaskProgress() const;

private:
    /// @brief buffer for request message data
    std::vector<std::uint8_t> request_data;
    /// @brief buffer for response message data
    std::vector<std::uint8_t> responce_data;
    /// @brief modbus protocol message generator
    modbus::ModbusClient modbus_client;
    /// @brief file control instance
    File file;
    /// @brief serial port instance
    sp::SerialPort serial_port;
    /// @brief vector with actual available modbus devices
    std::vector<ServerData> servers;
    /// @brief connected server index in servers
    int server_id = not_connected;
    /// @brief client-server data thread
    std::thread client_thread;
    /// @brief logic semaphore to stop client_thread
    std::atomic<bool> thread_stop{false};
    /// @brief async client task variable
    std::future<void> task;
    /// @brief info about actual pending task and function
    TaskInfo task_info = TaskInfo(ClientTasks::undefined, 0);
    /// @brief queue with client-server exchanges
    std::queue<std::function<void()>> q_exchange;
    /// @brief queue with client tasks
    std::queue<std::function<void()>> q_task;
    /// @brief read file from the server with passed id
    /// @param file_id file id
    void readFile(const ServerFiles file_id);
    /// @brief write file with to server
    /// @param file_id file id
    void writeFile(const ServerFiles file_id);
    /// @brief ping command
    void ping();
    /// @brief write record in file with new data
    /// @param file_id  file index
    /// @param record_id record index in file
    /// @param data new record data
    void writeRecord(const std::uint16_t file_id, const std::uint16_t record_id,
                     const std::vector<std::uint8_t>& data);
    /// @brief read record from file
    /// @param file_id  file index
    /// @param record_id record index in file
    /// @param length length to read
    void readRecord(const std::uint16_t file_id, const std::uint16_t record_id,
                    const std::uint16_t length);
    /// @brief write register command
    /// @param address address of register
    /// @param value register value
    void writeRegister(const std::uint16_t address, const std::uint16_t value);
    /// @brief read registers command
    /// @param address start address to read
    /// @param quantity amount of registers to read
    void readRegisters(const std::uint16_t address,
                       const std::uint16_t quantity);
    /// @brief create new server instance
    /// @param address server address
    void addServer(const std::uint8_t address);
    /// @brief handler for client_thread
    void clientThread();
    /// @brief async call for callServerExchange method
    /// @param attr new task attributes
    void createServerRequest(const TaskAttributes& attr);
    /// @brief call request/response exchange on data prepared in request_data
    void callServerExchange();
    /// @brief callback called for every function in q_exchange
    void exchangeCallback();
};
} // namespace sm

#endif // SM_CLIENT_H
