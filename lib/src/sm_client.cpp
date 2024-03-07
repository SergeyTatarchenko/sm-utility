/**
 * @file sm_client.cpp
 *
 * @brief 
 *
 * @author Siarhei Tatarchanka
 *
 */

#include <iostream>
#include <algorithm>
#include "../inc/sm_client.hpp"

namespace sm
{
    Client::Client()
    {
        client_thread = std::make_unique<std::thread>(std::thread(&Client::clientThread,this));
    }
    
    Client::~Client()
    {
        thread_stop.store(true, std::memory_order_relaxed);
        client_thread->join();
    }

    std::error_code& Client::start(std::string device)
    {
        if(serial_port.getState() != sp::PortState::Open)
        {
            task_info.error_code = serial_port.open(device);
        }
        else
        {
            if(device != serial_port.getPath())
            {
                serial_port.port.closePort();
                task_info.error_code = serial_port.open(device);
            }
        }
        return task_info.error_code;
    }

    std::error_code& Client::configure(sp::PortConfig config)
    {
        task_info.error_code = serial_port.setup(config);
        return task_info.error_code;
    }

    void Client::connect(const std::uint8_t address)
    {
        //server is not selected now
        if(server_id == not_connected){addServer(address);}
        //current server has different address
        if(servers[server_id].info.addr != address){addServer(address);}
        
        //ping server if it is not connected
        if(servers[server_id].info.status != ServerStatus::Available)
        {
            task_info = TaskInfo(ClientTasks::ping,1);
            auto lambda = [this]() 
            {
                q_exchange.push([this]{ping();});
            };
            q_task.push(lambda);
            while(!task_info.done);
        }
        if(servers[server_id].info.status == ServerStatus::Available)
        {
            //if ping success -> load info
        }
    }

    void Client::disconnect()
    {
        if(server_id != not_connected)
        {
            servers[server_id].info.status = ServerStatus::Unavailable;
            server_id = not_connected;
        }
    }

    void Client::addServer(const std::uint8_t address)
    {
        auto it = std::find_if(servers.begin(),servers.end(),[]( ServerData& server){return server.info.status == ServerStatus::Unavailable;} );
        if(it != servers.end())
        {
            //use existed slot
            int index =  std::distance(servers.begin(), it);
            servers[index] = ServerData();
            servers[index].info.addr = address;
            server_id = index;
        }
        else
        {
            //create new one
            servers.push_back(ServerData());
            servers.back().info.addr = address;
            server_id = servers.size() - 1;
        }
    }

    void Client::clientThread()
    {
        using namespace std::chrono_literals;
        while (!thread_stop.load(std::memory_order_relaxed))
        {
            while(!q_task.empty())
            {
                q_task.front()();
                while(!q_exchange.empty())
                {
                    try
                    {
                        q_exchange.front()();
                        task.wait();
                    }
                    catch(const std::system_error& e)
                    {
                        std::cerr << e.what() << std::endl;
                    }
                    exchangeCallback();
                    if(task_info.error == true)
                    {
                        std::queue<std::function<void()>> empty;
                        std::swap(q_exchange,empty);
                    }else{q_exchange.pop();}
                }
                q_task.pop();
            }
            std::this_thread::sleep_for(50ms);
        }
    }

    void Client::writeRecord(const std::uint16_t file_id, const std::uint16_t record_id, const std::vector<std::uint8_t>& data)
    {
        if(server_id != not_connected)
        {
            request_data = modbus_client.msgWriteFileRecord(servers[server_id].info.addr,file_id,record_id,data);
            // in case of success we expect message with the same length
            TaskAttributes attr = TaskAttributes(modbus::FunctionCodes::write_file,request_data.size()); 
            createServerRequest(attr);
        }
    }

    void Client::readRecord(const std::uint16_t file_id, const std::uint16_t record_id, const std::uint16_t length)
    {
        if(server_id != not_connected)
        {
            request_data = modbus_client.msgReadFileRecord(servers[server_id].info.addr,file_id,record_id,length);
            // amount of half words + 1 byte for ref type + 1 byte for data length 
            // + 1 byte for resp length + 1 byte for func + modbus required part
            TaskAttributes attr = TaskAttributes(modbus::FunctionCodes::read_file,static_cast<size_t>((length * 2) + 4)); 
            createServerRequest(attr);
        }
    }

    void Client::writeRegister(const std::uint16_t address, const std::uint16_t value)
    {
        if(server_id != not_connected)
        {
            request_data = modbus_client.msgWriteRegister(servers[server_id].info.addr,address,value);
            // in case of success we expect message with the same length
            TaskAttributes attr = TaskAttributes(modbus::FunctionCodes::write_register,request_data.size()); 
            createServerRequest(attr);
        }
    }

    void Client::readRegisters(const std::uint16_t address, const std::uint16_t quantity)
    {
        if(server_id != not_connected)
        {
            request_data = modbus_client.msgReadRegisters(servers[server_id].info.addr,address,quantity);
            // amount of 16 bit registers + 1 byte for length + 1 byte for func + modbus required part
            TaskAttributes attr = TaskAttributes(modbus::FunctionCodes::read_registers,static_cast<size_t>(getModbusRequriedLength() + (quantity * 2) + 2)); 
            createServerRequest(attr);
        }
    }

    void Client::ping()
    {
        if(server_id != not_connected)
        {
            std::uint8_t address  = servers[server_id].info.addr;
            std::uint8_t function = static_cast<uint8_t>(modbus::FunctionCodes::undefined);
            std::vector<uint8_t> message{0x00,0x00,0x00,0x00};
            request_data = modbus_client.msgCustom(address,function,message);
            // 1 byte for exception + 1 byte for func + modbus required part
            TaskAttributes attr = TaskAttributes(modbus::FunctionCodes::undefined,static_cast<size_t>(getModbusRequriedLength() + 2)); 
            createServerRequest(attr);
        }
    }
    
    void Client::exchangeCallback()
    {
        ++task_info.counter;
        if(modbus_client.isChecksumValid(responce_data))
        {
            switch(task_info.task)
            {
                case ClientTasks::ping:
                    if(server_id != not_connected)
                    {
                        if(responce_data.size() == task_info.attributes.length)
                        {
                            servers[server_id].info.status = ServerStatus::Available;
                            std::printf("connected to server : 0x%x \n", servers[server_id].info.addr);
                        }
                    }
                    break;
                
                default:
                    break;
            }
            if(task_info.counter == task_info.num_of_exchanges){task_info.done = true;}
        }
        else
        {
            task_info.error = true;
            task_info.done = true;
        }
    }
    
    void Client::createServerRequest(const TaskAttributes& attr)
    {
        task_info.attributes = attr;
        task = std::async(&Client::callServerExchange,this);
    }

    void Client::callServerExchange()
    {
        responce_data.clear();
        //write to server
        try{serial_port.port.writeBinary(request_data);}
        catch(const std::system_error& e){task_info.error_code = e.code();}
        //read from server
        try{serial_port.port.readBinary(responce_data,task_info.attributes.length);}
        catch(const std::system_error& e){task_info.error_code = e.code();}
    }
    
    std::uint8_t sm::Client::getModbusRequriedLength() const
    {
        std::uint8_t length;
        switch(modbus_client.getMode())
        {
            case modbus::ModbusMode::rtu:
                length = modbus::rtu_adu_size;
                break;
            
            case modbus::ModbusMode::ascii:
                length = modbus::ascii_adu_size;
                break;
            
            default:
                length = 0;
                break;
        }
        return length;
    }
}