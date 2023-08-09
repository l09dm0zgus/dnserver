#include <stdio.h>
#include "ThreadPool.h"
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 53
#define BUFFER_SIZE 512

void worker(void *arg)
{

}

int main()
{
    int socketDescriptor  = 0;
    ThreadPool *pool;
    pool = poolCreate(0);
    struct sockaddr_in serverAddress, clientAddress;

    char serverMessage[BUFFER_SIZE], clientMessage[BUFFER_SIZE];
    int clientStructLength = sizeof(clientAddress);

    // Clean buffers:
    memset(serverMessage, '\0', sizeof(serverMessage));
    memset(clientMessage, '\0', sizeof(clientMessage));

    // Create UDP socket:
    socketDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(socketDescriptor < 0)
    {
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind to the set port and IP:
    if(bind(socketDescriptor, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");
    struct timeval readTimeout;
    readTimeout.tv_sec = 0;
    readTimeout.tv_usec = 10;
    setsockopt(socketDescriptor, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, sizeof readTimeout);


    while (1)
    {
        int readedBytes = 0;
        readedBytes = recvfrom(socketDescriptor, clientMessage, sizeof(clientMessage), 0, (struct sockaddr*)&clientAddress, &clientStructLength);
        if(readedBytes > 0)
        {
            printf("Received message from IP: %s and port: %i\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));

            printf("Msg from client: %s\n", clientMessage);
        }


        // Respond to client:
        //strcpy(server_message, client_message);

        //if (sendto(socket_desc, server_message, strlen(server_message), 0,
          //         (struct sockaddr*)&client_addr, client_struct_length) < 0){
            //printf("Can't send\n");
            //return -1;
        //}
    }

    close(socketDescriptor);
    poolDestroy(pool);

    return 0;
}
