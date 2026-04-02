#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define PSK "secretkey146"
#define KEY 'X'

//xor encryption and decryption function
void encrypt_decrypt(char *data, int len) {
    for (int i = 0; i < len; i++) {
        data[i] ^= KEY;
    }
}

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

    //user authentication 
    char username[50], password[50], credentials[100];
    
    printf("Enter username: ");
    scanf("%49s", username);
    printf("Enter password: ");
    scanf("%49s", password);

    getchar(); 
// Combine username and password into a single string and encrypt it before sending to the server
    snprintf(credentials, sizeof(credentials), "%s:%s", username, password);
    encrypt_decrypt(credentials, strlen(credentials));
    send(sock, credentials, strlen(credentials), 0);

    //read server response
    int auth_len = read(sock, buffer, BUFFER_SIZE);
    encrypt_decrypt(buffer, auth_len);
    buffer[auth_len] = '\0';

    if (strcmp(buffer, "AUTH_OK") != 0) {
        printf("[-] Server rejected the authentication.\n");
        close(sock);
        return 1;
    }

    printf("[+] Authenticated with the server.\n");

    // Send message to server
    char message[BUFFER_SIZE];
    printf("Enter message to send: ");
    fgets(message, sizeof(message), stdin);
    encrypt_decrypt(message, strlen(message));
    send(sock, message, strlen(message), 0);

    // Receive response from server
    int response_len = read(sock, buffer, BUFFER_SIZE);
    encrypt_decrypt(buffer, response_len);
    buffer[response_len] = '\0';
    printf("Server: %s\n", buffer);

    // Close socket
    close(sock);

    return 0;
}