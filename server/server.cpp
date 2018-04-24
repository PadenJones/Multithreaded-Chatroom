/***********************************************************************
* Program:
*    Final Project - Server
*    Brother Jones, CS 460
* Author:
*    Paden Jones
* Summary:
*    Server half of a messaging application.
* Instructions:
*    Compile using Pthreads. Pass the port number as an argument.
************************************************************************/
#include <iostream>

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_LENGTH 512
#define MAX_CLIENTS 128

struct MessengerData
{
   long threadId;
   long clientId;
};

bool writeToSocket(int target, std::string message);
void* messenger(void* params);

pthread_t clients[MAX_CLIENTS];

pthread_mutex_t clientsMutex;
pthread_mutex_t broadcastMutex;
pthread_mutex_t ioMutex;
sem_t clientsFull;

int clientSocketIds[MAX_CLIENTS];
std::string clientNames[MAX_CLIENTS];
bool busyClients[MAX_CLIENTS];

// Returns first open index to house new messenger thread
int getNextIndex()
{
   for (int i = 0; i < MAX_CLIENTS; i++)
   {
      if (!busyClients[i])
      {
         busyClients[i] = 1;
         clientSocketIds[i] = -1;
         return i;
      }
   }
   
   return -1;
}

/******************************************************************************
* Main - sets up sockets, waits for clients to connects, spawns a new thread
*  for each new client.
******************************************************************************/
int main(int argc, char *argv[])
{
   int socketFileDescriptor;
   int portNumber;
   
   socklen_t clilen;
   
   char buffer[256];
   struct sockaddr_in serverAddress;
   
   int n;
   
   if (argc < 2) {
       fprintf(stderr, "Error: you must pass a Port # in the aruments\n");
       exit(1);
   }
   
   socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
   
   if (socketFileDescriptor < 0)
   {
      std::cout << "Error: could not open socket..." << std::endl;
      return 0;
   }
      
   bzero((char*) &serverAddress, sizeof(serverAddress));
   portNumber = atoi(argv[1]);
   serverAddress.sin_family = AF_INET;
   serverAddress.sin_addr.s_addr = INADDR_ANY;
   serverAddress.sin_port = htons(portNumber);
   
   if (bind(socketFileDescriptor, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
   {
      std::cout << "Error: could not bind socket to port...\n" << std::endl;
      return 0;
   }
   else
   {
      std::cout << "Server: Socket bind successful, waiting for clients." << std::endl;
   }

   listen(socketFileDescriptor, 5);
   
   struct sockaddr_in cli_addr;
   clilen = sizeof(cli_addr);
   
   
   /***************************************************************************
   * Concurrency Things
   ***************************************************************************/
   srand(time(0));
   pthread_mutex_init(&clientsMutex, NULL);
   pthread_mutex_init(&broadcastMutex, NULL);
   pthread_mutex_init(&ioMutex, NULL);
   
   sem_init(&clientsFull, 0, MAX_CLIENTS);
   for (int i = 0; i < MAX_CLIENTS; i++)
   {
      clientSocketIds[i] = -1;
      busyClients[i] = 0;
   }
   
   /***************************************************************************
   * Main program loop
   ***************************************************************************/
   while (true)
   {
      sem_wait(&clientsFull);
      pthread_mutex_lock(&clientsMutex);
      
      int nextIndex = getNextIndex();
      
      if (nextIndex != -1)
      {
         // Create messenger stuff here
         MessengerData* data = new MessengerData;

         data->threadId = nextIndex;
         data->clientId = accept(socketFileDescriptor, (struct sockaddr*) &cli_addr, &clilen);
         
         clientSocketIds[nextIndex] = data->clientId;
         
         int status = pthread_create(&clients[nextIndex], NULL, messenger, (void*)data);

         if (status != 0)
         {
            std::cout << "Fatal error: could not create thread...." << std::endl;
            return 0;
         }
      }
      else
      {
         std::cout << "Fatal error: semaphores didn't work, no open clients" << std::endl;
         return 0;
      }

      pthread_mutex_unlock(&clientsMutex);
   }

   return 0;
}


/******************************************************************************
* Messenger - thread for each client. Gets input from client and broadcasts
*  it to all other clients.
******************************************************************************/
void* messenger(void* params)
{
   MessengerData* data = (MessengerData*)params;
   char buffer[BUFFER_LENGTH];
   int n = 0;

   bool first = true;
   
   while (true)
   {
      // Get message from client
      bzero(buffer, BUFFER_LENGTH);
      n = read(data->clientId, buffer, BUFFER_LENGTH);
      if (n > 0)
      {
         if (first)
         {
            std::cout << "Welcome: " << buffer << std::endl;
            clientNames[data->clientId] = std::string(buffer);
            first = false;
         }
         else
         {
            // Broadcast to all clients
            pthread_mutex_lock(&ioMutex);
            std::cout << clientNames[data->clientId] << ": " << buffer;
            pthread_mutex_unlock(&ioMutex);

            // Only one client is allowd to broadcast at a time.
            pthread_mutex_lock(&broadcastMutex);
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
               if (i != data->threadId && busyClients[i] == 1)
               {
                  writeToSocket(clientSocketIds[i], clientNames[data->clientId] + ": " + std::string(buffer));
               }
            }
            pthread_mutex_unlock(&broadcastMutex);
         }
      }
      else if (n == 0)
      {
         std::cout << "Fatal thread error: client " << data->clientId << " closed their socket..." << std::endl;
         close(data->clientId);
         busyClients[data->threadId] = 0;
         clientNames[data->clientId] = "";
         sem_post(&clientsFull);
         return 0;
      }
      else
      {
         std::cout << "Fatal thread error: could not read from client " << data->clientId << "'s socket..." << std::endl;
         close(data->clientId);
         busyClients[data->threadId] = 0;
         clientNames[data->clientId] = "";
         sem_post(&clientsFull);
         return 0;
      }
   }
}

/******************************************************************************
*  Write to Socket - writes a message to a specified client through the
*     socket.
******************************************************************************/
bool writeToSocket(int target, std::string message)
{
   return write(target, message.c_str(), message.size()) >= 0;
}