#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define PSK "secretkey146"

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Define server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept a client connection
    new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (new_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    //Authentication using psk
read(new_socket, buffer, BUFFER_SIZE);

if (strcmp(buffer, PSK) == 0) {
        printf("[+] authenticated successful.\n");
        send(new_socket, "AUTH_OK", 7, 0);
    } else {
        printf("[-] Authentication failed.\n");
        send(new_socket, "AUTH_FAIL", 9, 0);
        close(new_socket);
        close(server_fd);
        return 1;
    }
//Clear buffer for the next message
    memset(buffer, 0, BUFFER_SIZE);

    // Receive and respond to client message
    read(new_socket, buffer, BUFFER_SIZE);
    printf("Client: %s\n", buffer);
    send(new_socket, "Hello from server", strlen("Hello from server"), 0);

    // Close sockets
    close(new_socket);
    close(server_fd);

    return 0;
}