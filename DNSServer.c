//
// Created by cx9ps3 on 10.08.2023.
//
#include "DNSServer.h"
#include "ThreadPool.h"

#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <glob.h>
#include <assert.h>
#include <json.h>
#include <errno.h>
#include <bits/unistd_ext.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

enum
{
    SIZE_OF_DOMAIN_TYPE = 2,
    DOMAIN_START = 12,
    PORT = 53,
    SIZE_OF_DOMAIN_TOP_LEVEL = 128,
    SIZE_OF_DOMAIN_SECOND_LEVEL = 256,
    BUFFER_SIZE = 512,
};



typedef struct
{
    u8 topLevel[SIZE_OF_DOMAIN_TOP_LEVEL];
    u8 secondLevel[SIZE_OF_DOMAIN_SECOND_LEVEL];
    u8 type[SIZE_OF_DOMAIN_TYPE];
}DomainName;

typedef struct
{
    u8 name[SIZE_OF_DOMAIN_SECOND_LEVEL + SIZE_OF_DOMAIN_TOP_LEVEL];
    json_object *root;
}ZoneData;

typedef struct
{
    u16 id;
    u16 flags;
    u16 qcount;	/* question count */
    u16 ancount;	/* Answer record count */
    u16 nscount;	/* Name Server (Autority Record) Count */
    u16 adcount;	/* Additional Record Count */
} DNSHeader;

typedef struct
{
    u8 *name;
    u16 type;
    u16 class;
    u32 nameSize;
}DNSQuery;

typedef struct
{
    u16 name;
    u16 type;
    u16 class;
    u32 ttl;
    u16 length;
    u32 data;
}DNSBody;

typedef struct
{
    DNSHeader header;
    DNSQuery query;
    DNSBody body;
}DNSResponse;


struct WorkerArgs
{
    int socketDescriptor;
    int readedBytes;
    u8 buffer[BUFFER_SIZE];
    struct sockaddr* clientAddress;
    int clientStructLength;
};

ZoneData *loadZones(int *size)
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
        assert(result == 0);
    }

    ZoneData *data = (ZoneData*)malloc(sizeof(ZoneData) * globBuffer.gl_pathc);
    for(int i = 0;i < globBuffer.gl_pathc;i++)
    {
        data[i].root = json_object_from_file(globBuffer.gl_pathv[i]);
        json_object *name = json_object_object_get(data->root,"origin");

        memset(data[i].name,'\0', SIZE_OF_DOMAIN_SECOND_LEVEL + SIZE_OF_DOMAIN_TOP_LEVEL);
        memcpy(data[i].name,(u8*)json_object_get_string(name),SIZE_OF_DOMAIN_SECOND_LEVEL + SIZE_OF_DOMAIN_TOP_LEVEL);

    }
    *size = globBuffer.gl_pathc;
    return data;
 }


 ZoneData getZone(DomainName* domainName)
 {
    int count = 0;
    ZoneData *zones = loadZones(&count);
    ZoneData zone = {};
    u8 name[SIZE_OF_DOMAIN_SECOND_LEVEL + SIZE_OF_DOMAIN_TOP_LEVEL];
    memset(name,'\0', SIZE_OF_DOMAIN_SECOND_LEVEL + SIZE_OF_DOMAIN_TOP_LEVEL);

    strcat((char*)name,(char*)domainName->secondLevel);

    name[strlen(((char*)domainName->secondLevel))] = '.';

    strcat((char*)name,(char*)domainName->topLevel);

    if(zones != NULL)
    {
        for(int i = 0;i < count;i++)
        {

            if(strcmp(zones[i].name,name) == 0)
            {
                zone = zones[i];
                free(zones);
                return zone;
            }
        }
    }
    free(zones);
    return zone;

 }

DomainName getQuestionDomain(const u8* data,int readedBytes)
{
    DomainName domainName;

    u8 secondLevelLength = data[0];
    u8 topLevelLength = data[secondLevelLength + 1];

    memcpy(domainName.secondLevel,data + 1,secondLevelLength);
    memcpy(domainName.topLevel,data + secondLevelLength + 2 ,topLevelLength + 1);
    memcpy(domainName.type,data + 1 + topLevelLength + secondLevelLength + 2  ,SIZE_OF_DOMAIN_TYPE);

    printf("%s.%s Type: %X , %X\n",domainName.secondLevel,domainName.topLevel,domainName.type[0],domainName.type[1]);

    return domainName;
}

DNSHeader buildDNSHeader(const u8* data,DomainName *name)
{
    DNSHeader header;
    header.id = (data[1] << 8) + data[0] ;
    header.flags = htons(0x8180);
    header.qcount = htons(1);
    char *qt;
    if(name->type[0] == '\x00' && name->type[1] == '\x01')
    {
        qt = "a";
    }

    ZoneData zone = getZone(name);

    header.ancount = htons(1);
    header.nscount = htons(0);
    header.adcount = htons(0);

    return header;
}


DNSBody buildDNSBody(DomainName *name,DNSHeader *header)
{
    ZoneData zone = getZone(name);
    DNSBody body;

    char *qt;
    if (name->type[0] == '\x00' && name->type[1] == '\x01')
    {
        qt = "a";
    }

    json_object* object = json_object_object_get(zone.root,qt);

    body.name = htons(0xC00C);
    body.type = htons(1);
    body.class = htons(1);

    json_object *ttlObject = json_object_object_get(object, "ttl");
    body.ttl = htonl(json_object_get_int(ttlObject));

    body.length = htons(sizeof(body.data));

    json_object *addressObject = json_object_object_get(object, "value");
    char* address = (char*)json_object_get_string(addressObject);
    printf("Address: %s \n", address);
    body.data = inet_addr(address);

    return body;
}

DNSQuery buildDNSQuery(DomainName *name)
{
    DNSQuery query;
    u8 secondLevelLength = strlen(name->secondLevel);
    u8 topLevelLength = strlen(name->topLevel);
    query.nameSize = secondLevelLength + topLevelLength + 2;

    memset(query.name,'\0', query.nameSize);

    query.name[0] = secondLevelLength;
    memcpy(query.name + 1, name->secondLevel,secondLevelLength);

    query.name[secondLevelLength + 1] = topLevelLength;
    memcpy(query.name + secondLevelLength + 2, name->topLevel,topLevelLength);
    query.name[secondLevelLength + 2 + topLevelLength] = '\x00';

    query.type = 1;
    query.class = 1;

    return query;

}

DNSResponse buildResponse(const u8* data,int readedBytes)
{
    DNSResponse response;

    DomainName name = getQuestionDomain(data + DOMAIN_START,readedBytes);

    response.header = buildDNSHeader(data,&name);
    response.query = buildDNSQuery(&name);
    response.body = buildDNSBody(&name,&response.header);

    return response;
}

 void worker(void *arg)
{
    WorkerArgs  *workerArgs = (WorkerArgs*)arg;

    printf("Send to client\n");

    DNSResponse response = buildResponse(workerArgs->buffer,workerArgs->readedBytes);
    DNSHeader header = response.header;
    DNSQuery query = response.query;
    DNSBody body = response.body;

    char buffer[BUFFER_SIZE];
    u32 responseSize = sizeof(header)  + (sizeof(u8) * query.nameSize) + (sizeof(u16) * 2) + sizeof(body);

    memcpy(buffer,&header,sizeof(header));
    memcpy(buffer + sizeof(header),query.name,sizeof(u8) * query.nameSize);
    memcpy(buffer + sizeof(header) + sizeof(u8) * query.nameSize,&query.type,sizeof(u16));
    memcpy(buffer + sizeof(header) + sizeof(u8) * query.nameSize + sizeof(u16),&query.class,sizeof(u16));
    memcpy(buffer + sizeof(header) + sizeof(u8) * query.nameSize + sizeof(u16) * 2,&body,sizeof(body));

    if (sendto(workerArgs->socketDescriptor, buffer, responseSize, 0,workerArgs->clientAddress, workerArgs->clientStructLength) < 0)
    {
        printf("Can't send!The last error message is: %s\n", strerror(errno));
    }

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

