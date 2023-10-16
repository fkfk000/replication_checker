#include <iostream>
#include <cstdlib>
#include "checker_postgres_server.h"
bool IsEnvOk(const char* name, const char* val);


int main(int argc, char * const argv[])
{
    const char* slotname = std::getenv("SlotName");
    const char* pubname = std::getenv("PubName");
    if (!IsEnvOk("SlotName", slotname) || !IsEnvOk("PubName", pubname))
    {
        return -1;
    }

    auto param = parseParameter(argc, argv);
    PostgresServer server(argc, argv);
    server.identifySystem();
    server.setSlotandStartReplication(slotname, pubname);
    return 0;
}

bool IsEnvOk(const char* name, const char* val) {
    if (val != nullptr) {
        std::cout << "Value of " << name << " is: " << val << std::endl;
        return 1;
    }
    else {
        std::cout << name <<" is not set." << std::endl;
        return 0;
    }
}