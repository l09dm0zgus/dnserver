//
// Created by cx9ps3 on 10.08.2023.
//
#ifndef DNSSERVER_H
#define DNSSERVER_H

typedef struct DNSServer DNSServer;

DNSServer *createDNSServer(const char *ip,int port,int readTimeout);

void destroyServer(DNSServer* server);

_Noreturn void serve(DNSServer* server);

#endif //DNSSERVER_H