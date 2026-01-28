// web_chat_server.c - HTTP-based chat server accessible via web browsers
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>
#include<arpa/inet.h>
#include<ifaddrs.h>
#include<time.h>
#include <ctype.h>

#define MAX_CLIENTS 50
#define BUFFER_SIZE 4096
#define MAX_MESSAGES 100

typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    int id;
    char name[32];
    int is_websocket;
} client_t;

typedef struct {
    char username[32];
    char message[256];
    char timestamp[32];
} chat_message_t;

client_t *clients[MAX_CLIENTS];
chat_message_t chat_history[MAX_MESSAGES];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;
int message_count = 0;

// Add message to history
void add_message_to_history(const char* username, const char* message) {
    pthread_mutex_lock(&messages_mutex);
    
    if (message_count >= MAX_MESSAGES) {
        // Shift messages to make room for new one
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            chat_history[i] = chat_history[i + 1];
        }
        message_count = MAX_MESSAGES - 1;
    }
    
    strcpy(chat_history[message_count].username, username);
    strcpy(chat_history[message_count].message, message);
    
    // Add timestamp
    time_t now = time(0);
    struct tm *tm_info = localtime(&now);
    strftime(chat_history[message_count].timestamp, 32, "%H:%M:%S", tm_info);
    
    message_count++;
    pthread_mutex_unlock(&messages_mutex);
}

// Generate chat history HTML
void generate_chat_history_html(char *html_buffer, int buffer_size) {
    pthread_mutex_lock(&messages_mutex);
    
    strcpy(html_buffer, "");
    for (int i = 0; i < message_count; i++) {
        char msg_html[512];
        snprintf(msg_html, sizeof(msg_html), 
            "<div class='message'>"
            "<span class='time'>%s</span> "
            "<span class='username'>%s:</span> "
            "<span class='text'>%s</span>"
            "</div>\n",
            chat_history[i].timestamp,
            chat_history[i].username,
            chat_history[i].message);
        
        if (strlen(html_buffer) + strlen(msg_html) < buffer_size - 100) {
            strcat(html_buffer, msg_html);
        }
    }
    
    pthread_mutex_unlock(&messages_mutex);
}

// Send HTTP response
void send_http_response(int client_fd, const char* status, const char* content_type, const char* body) {
    char response[BUFFER_SIZE * 2];
    snprintf(response, sizeof(response),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, content_type, strlen(body), body);
    
    send(client_fd, response, strlen(response), 0);
}

// Generate the main chat page HTML
void generate_chat_page(char *html_buffer, int buffer_size, const char* server_ip) {
    char chat_messages[BUFFER_SIZE];
    generate_chat_history_html(chat_messages, sizeof(chat_messages));
    
    snprintf(html_buffer, buffer_size,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset='UTF-8'>\n"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
        "<title>WiFi Chat Room</title>\n"
        "<style>\n"
        "body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background: linear-gradient(135deg, #667eea 0%%, #764ba2 100%%); min-height: 100vh; }\n"
        ".container { max-width: 800px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); overflow: hidden; }\n"
        ".header { background: #4a5568; color: white; padding: 20px; text-align: center; }\n"
        ".chat-area { height: 400px; overflow-y: auto; padding: 20px; background: #f7fafc; border-bottom: 1px solid #e2e8f0; }\n"
        ".message { margin: 5px 0; padding: 8px; background: white; border-radius: 5px; box-shadow: 0 1px 2px rgba(0,0,0,0.1); }\n"
        ".time { color: #718096; font-size: 0.8em; }\n"
        ".username { font-weight: bold; color: #2d3748; }\n"
        ".text { color: #4a5568; }\n"
        ".input-area { padding: 20px; background: white; }\n"
        ".input-group { display: flex; gap: 10px; margin-bottom: 10px; }\n"
        "input[type='text'] { flex: 1; padding: 10px; border: 1px solid #cbd5e0; border-radius: 5px; font-size: 16px; }\n"
        "button { padding: 10px 20px; background: #4299e1; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }\n"
        "button:hover { background: #3182ce; }\n"
        ".status { text-align: center; padding: 10px; color: #718096; font-style: italic; }\n"
        ".client-count { background: #48bb78; color: white; padding: 5px 10px; border-radius: 15px; font-size: 0.8em; }\n"
        "@media (max-width: 600px) {\n"
        "  .input-group { flex-direction: column; }\n"
        "  .container { margin: 0; border-radius: 0; }\n"
        "}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<div class='container'>\n"
        "<div class='header'>\n"
        "<h1>📱 WiFi Chat Room</h1>\n"
        "<div>Server: %s:8080 | <span class='client-count' id='clientCount'>%d users online</span></div>\n"
        "</div>\n"
        "<div class='chat-area' id='chatArea'>\n"
        "%s\n"
        "</div>\n"
        "<div class='input-area'>\n"
        "<div class='input-group'>\n"
        "<input type='text' id='username' placeholder='Your name' maxlength='30'>\n"
        "<input type='text' id='messageInput' placeholder='Type your message...' maxlength='200'>\n"
        "<button onclick='sendMessage()'>Send</button>\n"
        "</div>\n"
        "<div class='status' id='status'>Enter your name and start chatting!</div>\n"
        "</div>\n"
        "</div>\n"
        "\n"
        "<script>\n"
        "let lastMessageCount = %d;\n"
        "let username = '';\n"
        "\n"
        "function sendMessage() {\n"
        "    const usernameInput = document.getElementById('username');\n"
        "    const messageInput = document.getElementById('messageInput');\n"
        "    \n"
        "    username = usernameInput.value.trim();\n"
        "    const message = messageInput.value.trim();\n"
        "    \n"
        "    if (!username) {\n"
        "        alert('Please enter your name first!');\n"
        "        usernameInput.focus();\n"
        "        return;\n"
        "    }\n"
        "    \n"
        "    if (!message) {\n"
        "        alert('Please enter a message!');\n"
        "        messageInput.focus();\n"
        "        return;\n"
        "    }\n"
        "    \n"
        "    // Send message via POST request\n"
        "    fetch('/send', {\n"
        "        method: 'POST',\n"
        "        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
        "        body: `username=${encodeURIComponent(username)}&message=${encodeURIComponent(message)}`\n"
        "    })\n"
        "    .then(response => response.text())\n"
        "    .then(data => {\n"
        "        messageInput.value = '';\n"
        "        messageInput.focus();\n"
        "        updateChat();\n"
        "    })\n"
        "    .catch(error => {\n"
        "        document.getElementById('status').textContent = 'Error sending message. Please try again.';\n"
        "    });\n"
        "}\n"
        "\n"
        "function updateChat() {\n"
        "    fetch('/messages')\n"
        "    .then(response => response.text())\n"
        "    .then(html => {\n"
        "        document.getElementById('chatArea').innerHTML = html;\n"
        "        // Scroll to bottom\n"
        "        const chatArea = document.getElementById('chatArea');\n"
        "        chatArea.scrollTop = chatArea.scrollHeight;\n"
        "    })\n"
        "    .catch(error => {\n"
        "        console.error('Error updating chat:', error);\n"
        "    });\n"
        "}\n"
        "\n"
        "// Handle Enter key\n"
        "document.getElementById('messageInput').addEventListener('keypress', function(event) {\n"
        "    if (event.key === 'Enter') {\n"
        "        sendMessage();\n"
        "    }\n"
        "});\n"
        "\n"
        "document.getElementById('username').addEventListener('keypress', function(event) {\n"
        "    if (event.key === 'Enter') {\n"
        "        document.getElementById('messageInput').focus();\n"
        "    }\n"
        "});\n"
        "\n"
        "// Auto-refresh chat every 2 seconds\n"
        "setInterval(updateChat, 2000);\n"
        "\n"
        "// Initial focus\n"
        "document.getElementById('username').focus();\n"
        "</script>\n"
        "</body>\n"
        "</html>",
        server_ip, client_count, chat_messages, message_count);
}

// Parse HTTP request to extract path and POST data
void parse_http_request(const char* request, char* method, char* path, char* post_data) {
    sscanf(request, "%s %s", method, path);
    
    // Extract POST data if present
    const char* content_start = strstr(request, "\r\n\r\n");
    if (content_start) {
        strcpy(post_data, content_start + 4);
    } else {
        post_data[0] = '\0';
    }
}

// URL decode function
void url_decode(char* dst, const char* src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Get server IP for display
char* get_server_ip() {
    static char server_ip[INET_ADDRSTRLEN];
    struct ifaddrs *interfaces, *interface;
    struct sockaddr_in *addr;
    
    strcpy(server_ip, "localhost");
    
    if (getifaddrs(&interfaces) == -1) {
        return server_ip;
    }
    
    for (interface = interfaces; interface != NULL; interface = interface->ifa_next) {
        if (interface->ifa_addr == NULL || interface->ifa_addr->sa_family != AF_INET) 
            continue;
        
        addr = (struct sockaddr_in *)interface->ifa_addr;
        
        if (strncmp(interface->ifa_name, "wlan", 4) == 0 || 
            strncmp(interface->ifa_name, "wlp", 3) == 0 ||
            strncmp(interface->ifa_name, "wifi", 4) == 0 ||
            strncmp(interface->ifa_name, "eth", 3) == 0 ||
            strncmp(interface->ifa_name, "enp", 3) == 0 ||
            strncmp(interface->ifa_name, "eno", 3) == 0) {
            
            inet_ntop(AF_INET, &(addr->sin_addr), server_ip, INET_ADDRSTRLEN);
            if (strcmp(server_ip, "127.0.0.1") != 0) {
                break;
            }
        }
    }
    
    freeifaddrs(interfaces);
    return server_ip;
}

// Handle HTTP client
void *handle_http_client(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    char method[16], path[256], post_data[BUFFER_SIZE];
    char response_body[BUFFER_SIZE * 2];
    
    // Read HTTP request
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close(client_fd);
        return NULL;
    }
    
    buffer[bytes_read] = '\0';
    parse_http_request(buffer, method, path, post_data);
    
    if (strcmp(path, "/") == 0) {
        // Serve main chat page
        generate_chat_page(response_body, sizeof(response_body), get_server_ip());
        send_http_response(client_fd, "200 OK", "text/html", response_body);
        
    } else if (strcmp(path, "/messages") == 0) {
        // Serve chat messages
        generate_chat_history_html(response_body, sizeof(response_body));
        send_http_response(client_fd, "200 OK", "text/html", response_body);
        
    } else if (strcmp(path, "/send") == 0 && strcmp(method, "POST") == 0) {
        // Handle message sending
        char username[64] = "", message[256] = "";
        
        // Parse POST data
        char *token = strtok(post_data, "&");
        while (token != NULL) {
            if (strncmp(token, "username=", 9) == 0) {
                url_decode(username, token + 9);
            } else if (strncmp(token, "message=", 8) == 0) {
                url_decode(message, token + 8);
            }
            token = strtok(NULL, "&");
        }
        
        if (strlen(username) > 0 && strlen(message) > 0) {
            add_message_to_history(username, message);
            printf(" %s: %s\n", username, message);
        }
        
        send_http_response(client_fd, "200 OK", "text/plain", "OK");
        
    } else {
        // 404 Not Found
        send_http_response(client_fd, "404 Not Found", "text/html", 
            "<h1>404 - Page Not Found</h1><p><a href='/'>Go to Chat</a></p>");
    }
    
    close(client_fd);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pthread_t thread_id;
    int port = 8080;
    
    printf("Web-Based Chat Server Starting...\n");
    printf("=====================================\n");
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Socket creation failed\n");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Binding failed\n");
        return -1;
    }
    
    if (listen(server_fd, 10) < 0) {
        printf("Listen failed\n");
        return -1;
    }
    
    char *server_ip = get_server_ip();
    printf("Server running successfully!\n\n");
    printf("Access the chat room from any device:\n");
    printf("   Phone/Tablet:  http://%s:%d\n", server_ip, port);
    printf("   Computer:      http://%s:%d\n", server_ip, port);
    printf("   Localhost:     http://localhost:%d\n", port);
    printf("\n");
    printf("Instructions:\n");
    printf("   1. Make sure devices are on the same WiFi network\n");
    printf("   2. Open a web browser on any device\n");
    printf("   3. Go to http://%s:%d\n", server_ip, port);
    printf("   4. Enter your name and start chatting!\n");
    printf("\n");
    printf("Server is ready! Press Ctrl+C to stop.\n");
    printf("=====================================\n\n");
    
    // Accept connections
    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            continue;
        }
        
        // Create thread to handle client
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        
        if (pthread_create(&thread_id, NULL, handle_http_client, client_fd_ptr) != 0) {
            close(client_fd);
            free(client_fd_ptr);
        } else {
            pthread_detach(thread_id);
        }
    }
    
    close(server_fd);
    return 0;
}
