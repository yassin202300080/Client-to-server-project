#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Define server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
// Connect to localhost using the loopback ip
    server_address.sin_addr.s_addr = INADDR_ANY; 

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send message to server
    char message[BUFFER_SIZE];
    printf("Enter message to send: ");
    fgets(message, sizeof(message), stdin);
    send(sock, message, strlen(message), 0);

    // Receive response from server
    read(sock, buffer, BUFFER_SIZE);
    printf("Server: %s\n", buffer);

    // Close socket
    close(sock);

    return 0;
}