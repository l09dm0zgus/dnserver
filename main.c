#include <stdio.h>
#include "DNSServer.h"
int main()
{
    DNSServer *server = createDNSServer("127.0.0.1",0,50);
    serve(server);
    destroyServer(server);
    return 0;
}
