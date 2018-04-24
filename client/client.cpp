/***********************************************************************
* Program:
*    Multithreaded Chat Room
* Author:
*    Paden Jones
* Summary:
*    Client half of a messaging application.
* Instructions:
*    Compile using Pthreads. Pass port number and IP address
*    of server machine as arguments.
************************************************************************/
#include <iostream>
#include <string>

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
#include <netdb.h> 

#define BUFFER_LENGTH 512

void* reader(void* params);
void* writer(void* params);

pthread_t readerThread;
pthread_t writerThread;

pthread_mutex_t ioMutex;

int readFromServer(int sockfd, char* buffer);
int writeToServer(int sockfd);
void quit(int sockfd);

std::string username;

/******************************************************************************
*  Main - creates sockets, handles server instructions
******************************************************************************/
int main(int argc, char *argv[])
{
   int sockfd, portno;

   struct sockaddr_in serverAddress;
   struct hostent* server;

   if (argc < 3) {
      std::cout << "Error: you must specify hostname and port in arguments" << std::endl;
      return 0;
   }

   portno = atoi(argv[2]);
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (sockfd < 0)
   {
      std::cout << "Error: could not open socket on local machine..." << std::endl;
      return 0;
   }      

   server = gethostbyname(argv[1]);
   
   if (server == NULL) {
      std::cout << "Error: no such host..." << std::endl;
      return 0;
   }
   
   bzero((char*) &serverAddress, sizeof(serverAddress));
   serverAddress.sin_family = AF_INET;
   bcopy((char*) server->h_addr, (char*) &serverAddress.sin_addr.s_addr, server->h_length);
   
   serverAddress.sin_port = htons(portno);
   
   pthread_mutex_init(&ioMutex, NULL);
   
   // Connect to host
   if (connect(sockfd, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0)
   {
      std::cout << "Error: could not connect to host socket..." << std::endl;
      return 0;
   }   

   // Get username
   bool success = false;
   do
   {
      char cont;
      std::cout << "Enter a username: ";
      std::getline(std::cin, username);
      
      std::cout << "Your username is: " << username << ". Continue? (y/n) ";
      std::cin >> cont;

      if (cont == 'y')
      {
         success = true;
      }
      
      std::cin.clear();
      std::cin.ignore(10000, '\n');
   }
   while (!success);
   
   write(sockfd, username.c_str(), username.length());
   
   std::cout << "\n------------------------------------------------------" << std::endl;
   std::cout << "  Welcome to the chat. To open the send message" << std::endl;
   std::cout << "  dialogue you must first hit the enter key, then" << std::endl;
   std::cout << "  you can type your message." << std::endl;
   std::cout << "\n  The client cannot receive messages while you're" << std::endl;
   std::cout << "  typing, so be quick. Send message 'c' to cancel" << std::endl;
   std::cout << "  your message or 'q' to quit." << std::endl;
   std::cout << "------------------------------------------------------" << std::endl;
   
   // Spawn read thread
   int creationError = pthread_create(&readerThread, NULL, reader, (void*)(long)sockfd);
   if (creationError != 0)
   {
      std::cout << "Fatal error: could not create reader thread..." << std::endl;
      return 0;
   }
   
   // Spawn write thread
   creationError = pthread_create(&writerThread, NULL, writer, (void*)(long)sockfd);
   if (creationError != 0)
   {
      std::cout << "Fatal error: could not create writer thread..." << std::endl;
      return 0;
   }
   
   pthread_join(readerThread, NULL);
   pthread_join(writerThread, NULL);

   quit(sockfd);
   
   return 0;
}

void* reader(void* params)
{
   int status = 0;
   int sockfd = (long)params;
   char buffer[BUFFER_LENGTH];

   while (true)
   {
      status = readFromServer(sockfd, buffer);
      
      pthread_mutex_lock(&ioMutex);
      
      if (status > 0)
      {
         std::cout << buffer;
      }
      else if (status == 0)
      {
         std::cout << "Fatal error: the server has closed its socket..." << std::endl;
         quit(sockfd);
      }
      else
      {
         std::cout << "Fatal error: could not read from the server..." << std::endl;
         quit(sockfd);
      }
      
      pthread_mutex_unlock(&ioMutex);
   }
   
   return 0;
}

void* writer(void* params)
{
   int status = 0;
   int sockfd = (long)params;
   
   while (true)
   {
      status = writeToServer(sockfd);

      if (status < 0)
      {
         std::cout << "Fatal Error: error writing to server socket." << std::endl;
         quit(sockfd);
      }
      else if (status == 0)
      {
         std::cout << "Fatal Error: server has closed its connection." << std::endl;
         quit(sockfd);
      }
   }
   
   return 0;
}


/******************************************************************************
*  Read from Server - reads an incoming message from the server
******************************************************************************/
int readFromServer(int sockfd, char* buffer)
{
   bzero(buffer, BUFFER_LENGTH);
   return read(sockfd, buffer, BUFFER_LENGTH - 1);
}


/******************************************************************************
*  Write to Server - prompts the user for input, sends the input to the
*     server.
******************************************************************************/
int writeToServer(int sockfd)
{
   char buffer[BUFFER_LENGTH];
   bzero(buffer, BUFFER_LENGTH);

   std::cin.clear();
   std::cin.ignore(10000, '\n');

   pthread_mutex_lock(&ioMutex);
   fflush(stdin);
   std::cout << username << " (you): ";
   fgets(buffer, BUFFER_LENGTH - 1, stdin);

   pthread_mutex_unlock(&ioMutex);

   if (buffer[0] == 'c' && buffer[1] == '\n')
   {
      return 1;
   }
   else if (buffer[0] == 'q' && buffer[1] == '\n')
   {
      std::cout << "Goodbye!" << std::endl;
      quit(sockfd);
   }
   else
   {
      return write(sockfd, buffer, BUFFER_LENGTH);
   }
}

/******************************************************************************
*  Quit - closes socket and quits program
******************************************************************************/
void quit(int sockfd)
{
   close(sockfd);
   exit(0);
}