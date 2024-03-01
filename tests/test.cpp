/**
 * @file read_write.hpp
 *
 * @brief 
 *
 * @author Siarhei Tatarchanka
 *
 */

#include <iostream>
#include "../inc/sm_client.hpp"

int main(void)
{
    std::uint8_t address   = 42;
    std::uint16_t reg      = 1;
    std::uint16_t quantity = 10;

    modbus::ModbusClient client;
    
    std::vector<std::uint8_t> mes = client.msgReadRegisters(address,reg,quantity);
    
    std::cout<<"RTU mode "<<"\n";
    std::cout<<"msg size "<<mes.size()<<"\n";
    
    for(auto& byte : mes)
    {
        std::cout<<"0x"<<std::hex<<int(byte)<<" ";
    }
    std::cout<<"\n";
    client.setMode(modbus::ModbusMode::ascii);
    mes = client.msgReadRegisters(address,reg,quantity);
    std::cout<<"ASCII mode "<<"\n";
    std::cout<<"msg size "<<mes.size()<<"\n";
    
    for(auto& byte : mes)
    {
        std::cout<<"0x"<<std::hex<<int(byte)<<" ";
        //std::printf("0x%x ",byte);
    }

    return 0;
}
