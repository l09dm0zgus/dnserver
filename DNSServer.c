//
// Created by cx9ps3 on 10.08.2023.
//
#include "DNSServer.h"
#include "ThreadPool.h"

#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <glob.h>
#include <assert.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define PORT 53
#define BUFFER_SIZE 512



typedef struct
{
    u8 topLevel[128];
    u8 secondLevel[256];
    u8 type[2];
}DomainName;


typedef struct
{
    u16 id;
    u16 qr:1;
	u16 opcode:4;
	u16 aa:1;
	u16 tc:1;
	u16 rd:1;
	u16 ra:1;
	u16 zero:3;
	u16 rcode:4;
    u16 qcount;	/* question count */
    u16 ancount;	/* Answer record count */
    u16 nscount;	/* Name Server (Autority Record) Count */
    u16 adcount;	/* Additional Record Count */
} DNSHeader;

struct WorkerArgs
{
    int socketDescriptor;
    int readedBytes;
    u8 buffer[BUFFER_SIZE];
    struct sockaddr* clientAddress;
    int clientStructLength;
};

static void setFlags(const char* data,DNSHeader *header)
{
    char byte1 = data[2];
    char byte2 = data[3];
    header->qr = 0b1;
    for(u_int8_t i = 1; i <= 5;i++)
    {
        header->opcode += byte1 & (1<<i);
    }
    header->aa = 0b1;
    header->tc = 0b0;
    header->rd = 0b0;
    header->ra = 0b0;
    header->zero = 0b000;
    header->rcode = 0b0000;

}
 static void loadZones()
 {
    glob_t globBuffer;
    int result = glob("zones/*.zone",GLOB_ERR,NULL,&globBuffer);
    if(result == GLOB_NOMATCH)
    {
        printf("File by given pattern not found!\n");
    }
    else if(result == GLOB_ABORTED)
    {
        printf("Failed to read!");
    }
    assert(result == 0);


 }

DomainName getQuestionDomain(const u8* data,int readedBytes)
{
    DomainName domainName;

    u8 secondLevelLength = data[0];
    u8 topLevelLength = data[secondLevelLength + 1];

    memcpy(domainName.secondLevel,data + 1,secondLevelLength);
    memcpy(domainName.topLevel,data + secondLevelLength + 1 ,topLevelLength + 1);
    memcpy(domainName.type,data + 1 + topLevelLength + secondLevelLength + 2  ,2);

    printf("%s.%s Type: %X , %X\n",domainName.secondLevel,domainName.topLevel,domainName.type[0],domainName.type[1]);

    return domainName;
}

static DNSHeader buildResponse(const u8* data,int readedBytes)
{
    DNSHeader header;
    char transactionID[2];
    transactionID[0] = data[0];
    transactionID[1] = data[1];
    header.id = (transactionID[1] << 8) + transactionID[0];
    header.qcount = 1;

    getQuestionDomain(data+12,readedBytes);

    return header;

}

 void worker(void *arg)
{
    WorkerArgs  *workerArgs = (WorkerArgs*)arg;
    printf("Send to client\n");
    buildResponse(workerArgs->buffer,workerArgs->readedBytes);
    // Respond to client:
    //strcpy(server_message, client_message);

    //if (sendto(socket_desc, server_message, strlen(server_message), 0,
    //         (struct sockaddr*)&client_addr, client_struct_length) < 0){
    //printf("Can't send\n");
    //return -1;
    //}
}

struct DNSServer
{
    int socketDescriptor;
    ThreadPool *pool;
    struct sockaddr_in address;
    int port;
    u8 serverMessage[BUFFER_SIZE];
    u8 clientMessage[BUFFER_SIZE];
};

DNSServer *createDNSServer(const char *ip,int port,int readTimeout)
{
    DNSServer *server = malloc(sizeof(DNSServer));

    server->pool = poolCreate(0);
    if(port == 0)
    {
        server->port = PORT;
    }
    else
    {
        server->port = port;
    }

    // Clean buffers:
    memset(server->serverMessage, '\0', sizeof(server->serverMessage));
    memset(server->clientMessage, '\0', sizeof(server->clientMessage));

    // Create UDP socket:
    server->socketDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(server->socketDescriptor < 0)
    {
        printf("Error while creating socket\n");
        return NULL;
    }

    printf("Socket created successfully\n");

    // Set port and IP:
    server->address.sin_family = AF_INET;
    server->address.sin_port = htons(server->port);
    server->address.sin_addr.s_addr = inet_addr(ip);

    // Bind to the set port and IP:
    if(bind(server->socketDescriptor, (struct sockaddr*)&server->address, sizeof(server->address)) < 0)
    {
        printf("Couldn't bind to the port\n");
        return NULL;
    }
    printf("Done with binding\n");

    //set timeout
    struct timeval readSocketTimeout;
    readSocketTimeout.tv_sec = 0;
    readSocketTimeout.tv_usec = readTimeout;
    setsockopt(server->socketDescriptor, SOL_SOCKET, SO_RCVTIMEO, &readSocketTimeout, sizeof readSocketTimeout);
    return server;
}

void destroyServer(DNSServer* server)
{
    if(server != NULL)
    {
        close(server->socketDescriptor);
        poolDestroy(server->pool);
        free(server);
    }
}

_Noreturn void serve(DNSServer* server)
{
    if(server != NULL)
    {
        struct sockaddr_in clientAddress;
        int clientStructLength = sizeof(clientAddress);

        while (1)
        {
            int readedBytes = 0;
            readedBytes = recvfrom(server->socketDescriptor, server->clientMessage, BUFFER_SIZE , 0, (struct sockaddr*)&clientAddress, &clientStructLength);
            if(readedBytes > 0)
            {
                printf("Received message from IP: %s and port: %i\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));

                printf("Msg from client: %s\n", server->clientMessage);
                if(server->pool != NULL)
                {
                    WorkerArgs args;
                    args.socketDescriptor = server->socketDescriptor;
                    args.clientAddress = (struct sockaddr*)&clientAddress;
                    for(int i = 0;i < readedBytes;i++)
                    {
                        args.buffer[i] = server->clientMessage[i];
                    }

                    args.readedBytes = readedBytes;
                    args.clientStructLength = clientStructLength;
                    addWork(server->pool,worker,&args);
                }
            }



        }
    }
}

