#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define KEY "1234567890123456"
#define IV  "1234567890123456"

int aes_encrypt(unsigned char *plaintext, int plain_len, unsigned char *ciphertext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len, cipher_len;
    EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, (unsigned char *)KEY, (unsigned char *)IV);
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plain_len);
    cipher_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    cipher_len += len;
    EVP_CIPHER_CTX_free(ctx);
    printf("[AES] encrypted %d bytes\n", plain_len);
    return cipher_len;
}

int aes_decrypt(unsigned char *ciphertext, int cipher_len, unsigned char *plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len, plain_len;
    EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, (unsigned char *)KEY, (unsigned char *)IV);
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, cipher_len);
    plain_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plain_len += len;
    EVP_CIPHER_CTX_free(ctx);
    printf("[AES] decrypted %d bytes\n", cipher_len);
    return plain_len;
}

void upload_file(int sock, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("File not found.\n");
        return;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char ok[BUFFER_SIZE];
    snprintf(ok, sizeof(ok), "OK %ld", size);
    unsigned char enc[BUFFER_SIZE];
    int len = aes_encrypt((unsigned char*)ok, strlen(ok), enc);
    send(sock, enc, len, 0);
    unsigned char buf[BUFFER_SIZE], ec[BUFFER_SIZE];
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        len = aes_encrypt(buf, len, ec);
        send(sock, ec, len, 0);
    }
    fclose(fp);
    
    // response 
    char resp[BUFFER_SIZE];
    len = recv(sock, resp, BUFFER_SIZE, 0);
    printf("[DEBUG] upload response len=%d\n", len);
    if (len > 0) {
        len = aes_decrypt((unsigned char*)resp, len, (unsigned char*)resp);
        resp[len] = '\0';
        printf("%s\n", resp);
    }
}

void download_file(int sock, const char *filename) {
    char resp[BUFFER_SIZE];
    int len = recv(sock, resp, BUFFER_SIZE, 0);
    if (len <= 0) return;
    len = aes_decrypt((unsigned char*)resp, len, (unsigned char*)resp);
    resp[len] = '\0';
    if (strncmp(resp, "OK", 2) != 0) {
        printf("%s\n", resp);
        return;
    }
    long size;
    sscanf(resp, "OK %ld", &size);
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("Cannot create file\n");
        return;
    }
    long received = 0;
    while (received < size) {
        len = recv(sock, resp, BUFFER_SIZE, 0);
        if (len <= 0) break;
        len = aes_decrypt((unsigned char*)resp, len, (unsigned char*)resp);
        fwrite(resp, 1, len, fp);
        received += len;
    }
    fclose(fp);
    printf("Downloaded %s (%ld bytes)\n", filename, size);
}

int main() {
    int sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};
    unsigned char encrypted[BUFFER_SIZE] = {0};

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
    snprintf(credentials, sizeof(credentials), "%s:%s", username, password);
    int enc_len = aes_encrypt((unsigned char *)credentials, strlen(credentials), encrypted);
    send(sock, encrypted, enc_len, 0);

    //read server response
    int auth_len = read(sock, buffer, BUFFER_SIZE);
    int dec_len = aes_decrypt((unsigned char *)buffer, auth_len, (unsigned char *)buffer);
    buffer[dec_len] = '\0';

    if (strcmp(buffer, "AUTH_OK") != 0) {
        printf("[-] Server rejected the authentication.\n");
        close(sock);
        return 1;
    }

    printf("[+] Authenticated with the server.\n");

    
    while (1) {
        char message[BUFFER_SIZE];
        printf("Enter message ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;
    if (strcmp(message, "exit") == 0) break;

    char cmd[50], arg[BUFFER_SIZE];
    sscanf(message, "%49s %[^\n]", cmd, arg);

    if (strcmp(cmd, "upload") == 0 && strlen(arg) > 0) {
        
        enc_len = aes_encrypt((unsigned char*)message, strlen(message), encrypted);
        send(sock, encrypted, enc_len, 0);
        upload_file(sock, arg);
        continue;
    }
    else if (strcmp(cmd, "download") == 0 && strlen(arg) > 0) {
        enc_len = aes_encrypt((unsigned char*)message, strlen(message), encrypted);
        send(sock, encrypted, enc_len, 0);
        download_file(sock, arg);
        continue;
    }
    else {
        
        enc_len = aes_encrypt((unsigned char*)message, strlen(message), encrypted);
        send(sock, encrypted, enc_len, 0);
        memset(buffer, 0, BUFFER_SIZE);
        int response_len = read(sock, buffer, BUFFER_SIZE);
        if (response_len <= 0) {
            printf("Server disconnected.\n");
            break;
        }
        dec_len = aes_decrypt((unsigned char*)buffer, response_len, (unsigned char*)buffer);
        buffer[dec_len] = '\0';
        printf("%s\n", buffer);
    }
    }
    close(sock);
    return 0;
}