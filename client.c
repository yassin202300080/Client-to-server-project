#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define PSK "secretkey146"

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
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    //authentication
    send(sock, PSK, strlen(PSK), 0);
    read(sock, buffer, BUFFER_SIZE);

    if (strcmp(buffer, "AUTH_OK") != 0) {
        printf("[-] Server rejected the authentication.\n");
        close(sock);
        return 1;
    }
    
    printf("[+] Authenticated with the server.\n");

    // Clear buffer for the next message
    memset(buffer, 0, BUFFER_SIZE);

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