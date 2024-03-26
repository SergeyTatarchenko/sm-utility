/**
 * @file sm_utility.hpp
 *
 * @brief 
 *
 * @author Siarhei Tatarchanka
 *
 */

#include <iostream>
#include "../inc/sm_client.hpp"

const std::string interactive_text = "program started in interactive mode, type help for available commands \n";
const std::string error_text = "unsupported command passed. \n";
const std::string input_start = ">";
const std::string help_text = "help";
const std::string exit_text = "exit";
const std::string scanport_text = "scanport";
const std::string status_text = "status";
const std::string start_text = "start";
const std::string connect_text = "connect";
const std::string disconnect_text = "disconnect";
const std::string upload_text = "upload";
const std::string erase_text = "erase";

enum class Commands
{
    unknown,
    help,
    exit,
    scanport,
    start,
    status,
    connect,
    disconnect,
    upload,
    erase
};

sm::ServerData server_data;
std::string port = "NULL";
std::string firmware_file = "NULL";
std::uint8_t server_address = 0;

static void print_devices(std::vector<std::string>& devices);
static void print_help();
static void print_status();
static Commands process_cmd(std::string& str);

static void print_devices(std::vector<std::string>& devices)
{
    std::cout<<"Available serial ports: \n";
    for(std::string& device : devices)
    {
        std::cout<<device<<"\n";
    }
}

void print_help()
{
    std::cout<<"Available commands : \n\n"
            <<help_text<<" - used for help text output; \n\n"
            <<exit_text<<" - used to exit from program; \n\n"
            <<status_text<<" - print actual client status; \n\n"
            <<scanport_text<<" - used for scan for available serial ports in system; \n\n"
            <<start_text<<" - start client on selected port, usage example : start COM1; \n\n"
            <<connect_text<<" - connect to server with passed id, usage example : connect 77; \n\n"
            <<disconnect_text<<" - disconnect from server; \n\n"
            <<upload_text<<" - upload new firmware to the server, usage example : upload firmware.bin; \n\n"
            <<erase_text<<" - erase firmware from server; \n\n"
            ;
}

void print_status()
{
    std::cout<<"used port : "<<port<<"\n";
    if(server_data.info.status == sm::ServerStatus::Available)
    {
        std::printf("connected to server with id : %d \n",server_address);
    }
    else
    {
        std::cout<<"server not connected.\n";
    }
}

int main(int argc, char* argv[])
{
    std::error_code error;
    sp::PortConfig config;

    //instance used to search for available serial ports
    sp::SerialDevice serial_device;
    //main client instance
    sm::Client client;
    //vector with available serial ports
    std::vector<std::string> devices;
    
    //hardcoded port parameters
    config.baudrate = sp::PortBaudRate::BD_19200;
    config.timeout_ms = 2000;

    if(argc == 1)
    {
        std::cout<<interactive_text;
        while(true)
        {
            std::string str;
            std::cout<<input_start;
            std::getline(std::cin, str);
            Commands cmd = process_cmd(str);
            switch(cmd)
            {
                case Commands::help:
                    print_help();
                    break;
                
                case Commands::status:
                    client.getServerData(server_data);
                    print_status();
                    break;

                case Commands::scanport:
                    serial_device.updateAvailableDevices();
                    devices = serial_device.getListOfAvailableDevices();
                    print_devices(devices);
                    break;

                case Commands::exit:
                    std::cout<<"program stopped, exit.\n";
                    return 0;
                
                case Commands::start:
                    error = client.start(port);
                    if(error)
                    {
                        std::cout<<"failed to start client. \n"; 
                        std::cout<<"error: "<<error.message()<<"\n";
                    }
                    error = client.configure(config);
                    if(error)
                    {
                        std::cout<<"failed to configure client. \n";
                        std::cout<<"error: "<<error.message()<<"\n";
                    }
                    else
                    {
                        std::cout<<"client started at "<<port<<" ...\n";
                    }
                    break;
                
                case Commands::upload:
                    error = client.uploadApp(firmware_file);
                    if(error)
                    {
                        std::printf("failed to upload firmware. \n");
                        std::cout<<"error: "<<error.message()<<"\n";
                    }
                    else
                    {
                        std::cout<<"new firmware uploaded.\n";
                    }
                    break;
                
                case Commands::disconnect:
                    client.disconnect();
                    client.getServerData(server_data);
                    std::printf("disconnected from server with id : %d \n",server_address);
                    server_address = 0;
                    break;
                
                case Commands::erase:
                    error = client.eraseApp(); 
                    if(error)
                    {
                        std::printf("failed to erase app on server. \n");
                        std::cout<<"error: "<<error.message()<<"\n";
                        return 0;
                    }
                    else
                    {
                         std::cout<<"firmware erased.\n";
                    }
                    break;

                case Commands::connect:
                    error = client.connect(server_address);
                    if(error)
                    {
                        std::printf("failed to conect to server. \n");
                        std::cout<<"error: "<<error.message()<<"\n";
                    }
                    else
                    {
                        client.getServerData(server_data);
                        std::printf("connected to server with id : %d \n",server_address);
                        std::printf("device name   : %s \n",server_data.data.boot_name);
                        std::printf("boot version  : %s \n",server_data.data.boot_version);
                        std::printf("available ROM : %d KB \n",server_data.data.available_rom/1024);
                    }
                    break;

                case Commands::unknown:
                default:    
                    std::cout<<error_text;
            }
        }
    }
    return 0;
}

Commands process_cmd(std::string& str) 
{    
    auto get_argv = [](std::vector<std::string>& argv, std::string& str)
    {
        std::string arg = "";
        for (auto x : str) 
        {
            if (x == ' ')
            {
                argv.push_back(arg);
                arg = "";
            }
            else {
                arg = arg + x;
            }
        }
        argv.push_back(arg);
    };

    Commands cmd = Commands::unknown;
    std::vector<std::string> argv;
    get_argv(argv,str);

    if(argv[0] == help_text)
    {
        cmd = Commands::help;
    }
    if(argv[0] == exit_text)
    {
        cmd = Commands::exit;
    }
    if(argv[0] == scanport_text)
    {
        cmd = Commands::scanport;
    }
    if(argv[0] == status_text)
    {
        cmd = Commands::status;
    }
    if(argv[0] == erase_text)
    {
        cmd = Commands::erase;
    }
    if(argv[0] == disconnect_text)
    {
        cmd = Commands::disconnect;
    }
    if(argv[0] == connect_text)
    {
        if(argv.size() == 2)
        {
            try
            {
                server_address = std::stoi(argv[1]);
                cmd = Commands::connect;
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
    }
    if(argv[0] == upload_text)
    {
        if(argv.size() == 2)
        {
            firmware_file = argv[1];
            cmd = Commands::upload;
        }
    }
    if(argv[0] == start_text)
    {
        if(argv.size() == 2)
        {
            port = argv[1];
            cmd = Commands::start;
        }
    }
    return cmd; 
}
