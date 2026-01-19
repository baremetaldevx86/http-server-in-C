// chat_server.c - Multi-client chat server using threads
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/ip.h>
#include<string.h>
#include<pthread.h>
#include<arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Structure to store client information
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    int id;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;

// Add client to the array
void add_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!clients[i]) {
            clients[i] = cl;
            client_count++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove client from the array
void remove_client(int id) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i]) {
            if(clients[i]->id == id) {
                clients[i] = NULL;
                client_count--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send message to all clients except sender
void broadcast_message(char *message, int sender_id) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i]) {
            if(clients[i]->id != sender_id) {
                if(write(clients[i]->socket_fd, message, strlen(message)) < 0) {
                    printf("Error sending message to client %d\n", clients[i]->id);
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send message to all clients (including from server)
void server_broadcast(char *message) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i]) {
            if(write(clients[i]->socket_fd, message, strlen(message)) < 0) {
                printf("Error sending message to client %d\n", clients[i]->id);
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send message to specific client
void send_to_client(char *message, int client_id) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i] && clients[i]->id == client_id) {
            if(write(clients[i]->socket_fd, message, strlen(message)) < 0) {
                printf("Error sending message to client %d\n", client_id);
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// List all connected clients
void list_clients() {
    pthread_mutex_lock(&clients_mutex);
    printf("\n=== Connected Clients ===\n");
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i]) {
            printf("Client %d: %s (%s:%d)\n", 
                clients[i]->id, 
                clients[i]->name,
                inet_ntoa(clients[i]->address.sin_addr),
                ntohs(clients[i]->address.sin_port));
        }
    }
    printf("Total clients: %d\n", client_count);
    printf("========================\n\n");
    pthread_mutex_unlock(&clients_mutex);
}

// Handle individual client communication
void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 32];
    int leave_flag = 0;
    client_t *cli = (client_t*)arg;
    
    // Get client name
    if(recv(cli->socket_fd, cli->name, sizeof(cli->name), 0) <= 0) {
        strcpy(cli->name, "Anonymous");
    }
    cli->name[sizeof(cli->name) - 1] = '\0'; // Ensure null termination
    
    sprintf(message, "%s has joined the chat!\n", cli->name);
    printf("%s", message);
    broadcast_message(message, cli->id);
    
    while(1) {
        if(leave_flag) {
            break;
        }
        
        int receive = recv(cli->socket_fd, buffer, BUFFER_SIZE, 0);
        if(receive > 0) {
            if(receive < BUFFER_SIZE) {
                buffer[receive] = '\0';
            }
            
            // Remove newline
            buffer[strcspn(buffer, "\n")] = 0;
            
            if(strlen(buffer) > 0) {
                sprintf(message, "%s: %s\n", cli->name, buffer);
                printf("%s", message);
                broadcast_message(message, cli->id);
            }
        } else if(receive == 0 || strcmp(buffer, "exit") == 0) {
            sprintf(message, "%s has left the chat.\n", cli->name);
            printf("%s", message);
            broadcast_message(message, cli->id);
            leave_flag = 1;
        } else {
            printf("Error receiving message from %s\n", cli->name);
            leave_flag = 1;
        }
    }
    
    // Clean up
    close(cli->socket_fd);
    remove_client(cli->id);
    free(cli);
    pthread_detach(pthread_self());
    
    return NULL;
}

// Server command handler thread
void *server_command_handler(void *arg) {
    char command[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    
    printf("\n=== Server Commands ===\n");
    printf("/broadcast <message> - Send message to all clients\n");
    printf("/list - List all connected clients\n");
    printf("/send <client_id> <message> - Send message to specific client\n");
    printf("/help - Show this help\n");
    printf("======================\n\n");
    
    while(1) {
        printf("Server> ");
        fflush(stdout);
        
        if(fgets(command, sizeof(command), stdin) == NULL) {
            continue;
        }
        
        command[strcspn(command, "\n")] = 0; // Remove newline
        
        if(strncmp(command, "/broadcast ", 11) == 0) {
            sprintf(message, "[SERVER]: %s\n", command + 11);
            server_broadcast(message);
            printf("Message broadcasted to all clients.\n");
        }
        else if(strcmp(command, "/list") == 0) {
            list_clients();
        }
        else if(strncmp(command, "/send ", 6) == 0) {
            int client_id;
            char *msg_start = strchr(command + 6, ' ');
            if(msg_start) {
                *msg_start = '\0';
                client_id = atoi(command + 6);
                msg_start++;
                sprintf(message, "[SERVER to you]: %s\n", msg_start);
                send_to_client(message, client_id);
                printf("Message sent to client %d.\n", client_id);
            } else {
                printf("Usage: /send <client_id> <message>\n");
            }
        }
        else if(strcmp(command, "/help") == 0) {
            printf("\n=== Server Commands ===\n");
            printf("/broadcast <message> - Send message to all clients\n");
            printf("/list - List all connected clients\n");
            printf("/send <client_id> <message> - Send message to specific client\n");
            printf("/help - Show this help\n");
            printf("======================\n\n");
        }
        else if(strlen(command) > 0) {
            printf("Unknown command. Type /help for available commands.\n");
        }
    }
    
    return NULL;
}

int main() {
    int Socket_Serv_FD, Socket_Client_FD;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pthread_t tid, server_tid;
    int port = 8080;
    
    // Initialize clients array
    for(int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = NULL;
    }
    
    printf("Multi-client chat server starting on port %d...\n", port);
    
    // Create socket
    Socket_Serv_FD = socket(AF_INET, SOCK_STREAM, 0);
    if(Socket_Serv_FD < 0) {
        printf("Socket creation failed.\n");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if(setsockopt(Socket_Serv_FD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Setting socket options failed.\n");
        return -1;
    }
    
    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if(bind(Socket_Serv_FD, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Binding failed.\n");
        return -1;
    }
    
    // Listen
    if(listen(Socket_Serv_FD, 5) < 0) {
        printf("Listen failed.\n");
        return -1;
    }
    
    printf("Server listening for connections...\n");
    
    // Start server command handler thread
    if(pthread_create(&server_tid, NULL, server_command_handler, NULL) != 0) {
        printf("Error creating server command thread.\n");
        return -1;
    }
    
    // Accept clients
    while(1) {
        client_len = sizeof(client_addr);
        Socket_Client_FD = accept(Socket_Serv_FD, (struct sockaddr*)&client_addr, &client_len);
        
        if(Socket_Client_FD < 0) {
            printf("Accept failed.\n");
            continue;
        }
        
        // Check if max clients reached
        if(client_count == MAX_CLIENTS) {
            printf("Max clients reached. Connection rejected.\n");
            close(Socket_Client_FD);
            continue;
        }
        
        // Create client structure
        client_t *cli = (client_t*)malloc(sizeof(client_t));
        cli->address = client_addr;
        cli->socket_fd = Socket_Client_FD;
        cli->id = Socket_Client_FD; // Using socket fd as unique id
        
        // Add client and create thread
        add_client(cli);
        pthread_create(&tid, NULL, handle_client, (void*)cli);
        
        printf("Client connected from %s:%d\n", 
            inet_ntoa(client_addr.sin_addr), 
            ntohs(client_addr.sin_port));
    }
    
    return 0;
}
