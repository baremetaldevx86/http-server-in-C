// client.c - Simple chat client
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<pthread.h>
#include<arpa/inet.h>

#define BUFFER_SIZE 1024

int socket_fd;
char name[32];

// Thread to receive messages from server
void *receive_messages(void *arg) {
    char message[BUFFER_SIZE];
    
    while(1) {
        int receive = recv(socket_fd, message, BUFFER_SIZE, 0);
        if(receive > 0) {
            message[receive] = '\0';
            printf("%s", message);
            fflush(stdout);
        } else if(receive == 0) {
            printf("Server disconnected.\n");
            break;
        } else {
            printf("Error receiving message.\n");
            break;
        }
    }
    
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    pthread_t receive_thread;
    char message[BUFFER_SIZE];
    
    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0) {
        printf("Socket creation failed.\n");
        return -1;
    }
    
    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // Connect to server
    if(connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection failed.\n");
        return -1;
    }
    
    printf("Connected to chat server!\n");
    
    // Get username
    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0; // Remove newline
    
    // Send name to server
    send(socket_fd, name, strlen(name), 0);
    
    printf("Welcome to the chat, %s!\n", name);
    printf("Type 'exit' to quit.\n\n");
    
    // Start receive thread
    if(pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        printf("Error creating receive thread.\n");
        return -1;
    }
    
    // Main loop for sending messages
    while(1) {
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0; // Remove newline
        
        if(strcmp(message, "exit") == 0) {
            break;
        }
        
        if(strlen(message) > 0) {
            send(socket_fd, message, strlen(message), 0);
        }
    }
    
    close(socket_fd);
    return 0;
}
