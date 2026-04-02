#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define KEY 'X'

//xor encryption and decryption function
void encrypt_decrypt(char *data, int len) {
    for (int i = 0; i < len; i++) {
        data[i] ^= KEY;
    }
}

int authenticate_user(const char *username, const char *password) {
    FILE *users_file = fopen("users.txt", "r");
    if (users_file == NULL) {
        perror("Could not open users.txt");
        return 0;
    }

    char file_username[50];
    char file_password[50];
    while (fscanf(users_file, "%49s %49s", file_username, file_password) == 2) {
        if (strcmp(username, file_username) == 0 && strcmp(password, file_password) == 0) {
            fclose(users_file);
            return 1;
        }
    }

    fclose(users_file);
    return 0;
}

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

    // Receive encrypted username:password
    int auth_len = read(new_socket, buffer, BUFFER_SIZE);
    if (auth_len <= 0) {
        printf("[-] No authentication data received.\n");
        close(new_socket);
        close(server_fd);
        return 0;
    }
    encrypt_decrypt(buffer, auth_len);
    buffer[auth_len] = '\0';

    char username[50];
    char password[50];
    if (sscanf(buffer, "%49[^:]:%49s", username, password) != 2 || !authenticate_user(username, password)) {
        printf("[-] Client authentication failed.\n");
        char auth_fail[] = "AUTH_FAIL";
        encrypt_decrypt(auth_fail, strlen(auth_fail));
        send(new_socket, auth_fail, strlen(auth_fail), 0);
        close(new_socket);
        close(server_fd);
        return 0;
    }

    printf("[+] Client authenticated successfully: %s\n", username);
    char auth_ok[] = "AUTH_OK";
    encrypt_decrypt(auth_ok, strlen(auth_ok));
    send(new_socket, auth_ok, strlen(auth_ok), 0);
//Clear buffer for the next message
    memset(buffer, 0, BUFFER_SIZE);
    int valread = read(new_socket, buffer, BUFFER_SIZE);

    //printing encrypted message from client
    printf("Encrypted message from client: %s\n", buffer);

    //printing decrypted message from client
    encrypt_decrypt(buffer, valread);
    printf("Decrypted message from client: %s\n", buffer);


    // Receive and respond to client message
    int msg_len = valread;
    buffer[msg_len] = '\0';
    printf("Client: %s\n", buffer);
    char reply[] = "Hello from server";
    encrypt_decrypt(reply, strlen(reply));
    send(new_socket, reply, strlen(reply), 0);

    // Close sockets
    close(new_socket);
    close(server_fd);

    return 0;
}