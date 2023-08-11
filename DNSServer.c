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
    char *name;
    json_object *root;
}ZoneData;

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

typedef struct
{
    u8 size1;
    u8 *name1;
    u8 size2;
    u8 *name2;
    u8 type[SIZE_OF_DOMAIN_TYPE];
}DNSQuery;

typedef struct
{
    u8 compression[2];
    u8 type[SIZE_OF_DOMAIN_TYPE];
    u32 ttl;
    u16 length;
    u8 *data;
}DNSBody;

typedef struct
{
    DNSHeader header;
    DNSQuery query;
    DNSBody *body;
}DNSResponse;


struct WorkerArgs
{
    int socketDescriptor;
    int readedBytes;
    u8 buffer[BUFFER_SIZE];
    struct sockaddr* clientAddress;
    int clientStructLength;
};

void setFlags(const u8* data,DNSHeader *header)
{
    u8 byte1 = data[2];
    u8 byte2 = data[3];

    header->qr = 0b1;
    header->opcode = 0b0;
    header->aa = 0b1;
    header->tc = 0b0;
    header->rd = 0b0;
    header->ra = 0b0;
    header->zero = 0b000;
    header->rcode = 0b0000;

}
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
        data[i].name = (char*)json_object_get_string(name);

    }
    *size = globBuffer.gl_pathc;
    return data;
 }

 ZoneData getZone(DomainName* domainName)
 {
    int count = 0;
    ZoneData *zones = loadZones(&count);
    ZoneData zone = {};

    if(zones != NULL)
    {
        for(int i = 0;i < count;i++)
        {
            char name[SIZE_OF_DOMAIN_SECOND_LEVEL + SIZE_OF_DOMAIN_TOP_LEVEL];

            strcat(name,(const char*)domainName->secondLevel);
            strcat(name,".");
            strcat(name,(const char*)domainName->topLevel);

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
    memcpy(domainName.topLevel,data + secondLevelLength + 1 ,topLevelLength + 1);
    memcpy(domainName.type,data + 1 + topLevelLength + secondLevelLength + 2  ,SIZE_OF_DOMAIN_TYPE);

    printf("%s.%s Type: %X , %X\n",domainName.secondLevel,domainName.topLevel,domainName.type[0],domainName.type[1]);

    return domainName;
}

DNSHeader buildDNSHeader(const u8* data,DomainName *name)
{
    DNSHeader header;
    u8 transactionID[2];
    transactionID[0] = data[0];
    transactionID[1] = data[1];
    header.id = (transactionID[1] << 8) + transactionID[0];

    setFlags(data, &header);

    u8 bytes[2];
    bytes[0] = '\x00';
    bytes[1] = '\x01';
    header.qcount = (bytes[1] << 8) + bytes[0];;

    char *qt;

    if(name->type[0] == '\x00' && name->type[1] == '\x01')
    {
        qt = "a";
    }

    ZoneData zone = getZone(name);

    if(zone.root != NULL)
    {
        json_object* object = json_object_object_get(zone.root,qt);
        if(object!= NULL)
        {
            header.ancount = json_object_array_length(object);
        }
    }
    else
    {
        header.ancount = 0;
    }

    header.nscount = 0;
    header.adcount = 0;

    return header;
}


DNSBody *buildDNSBody(DomainName *name,DNSHeader *header)
{
    ZoneData zone = getZone(name);
    DNSBody *body = (DNSBody*)malloc(sizeof(DNSBody) * header->ancount);

    char *qt;
    if (name->type[0] == '\x00' && name->type[1] == '\x01')
    {
        qt = "a";
    }

    json_object* object = json_object_object_get(zone.root,qt);
    for (int i = 0; i < header->ancount; ++i)
    {

        body[i].compression[0] = '\xc0';
        body[i].compression[1] = '\x0c';
        memcpy(body[i].type,name->type,SIZE_OF_DOMAIN_TYPE);


        json_object* arrayElement = json_object_array_get_idx(object, i);

        json_object* ttlObject = json_object_object_get(arrayElement,"ttl");
        body[i].ttl = json_object_get_int(ttlObject);

        json_object* addressObject = json_object_object_get(arrayElement,"value");
        const char* address = json_object_get_string(addressObject);

        if(strcmp(qt,"a") == 0)
        {
            body[i].length = 4;
        }

        body[i].data = (u8*) malloc(sizeof(u8) * body[i].length);

        char *token = strtok((char*)address, ".");
        int j = 0;
        while(token != NULL )
        {
            body[i].data[j] = (u8)atoi(token);
            j++;
            token = strtok(NULL, ".");
        }
    }

    return body;
}

DNSQuery buildDNSQuery(DomainName *name)
{
    DNSQuery query;
    memcpy(query.type , name->type,SIZE_OF_DOMAIN_TYPE);
    query.size1 = strlen((const char*)name->topLevel);
    query.size2 = strlen((const char*)name->secondLevel);

    query.name1 = name->topLevel;
    query.name2 = name->secondLevel;

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

    if (sendto(workerArgs->socketDescriptor, &response, sizeof(DNSResponse), 0,workerArgs->clientAddress, workerArgs->clientStructLength) < 0)
    {
        printf("Can't send!The last error message is: %s\n", strerror(errno));
    }
    for(int i = 0 ;i < response.header.ancount;i++)
    {
        free(response.body[i].data);
    }

    free(response.body);
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

