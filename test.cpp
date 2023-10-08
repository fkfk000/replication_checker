#include <iostream>
#include "checker_postgres_server.h"

int main(int argc, char * const argv[])
{
    auto param = parseParameter(argc, argv);
    PostgresServer server(argc, argv);
    server.identifySystem();
    server.setSlotandStartReplication("sub", "pub");
    return 0;
}