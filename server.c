#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define KEY "1234567890123456"
#define IV  "1234567890123456"

typedef enum { ENTRY, MEDIUM, TOP } role_t;

int aes_encrypt(unsigned char *plain, int plain_len, unsigned char *cipher) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, KEY, IV);
    int len, cipher_len = 0;
    EVP_EncryptUpdate(ctx, cipher, &len, plain, plain_len);
    cipher_len = len;
    EVP_EncryptFinal_ex(ctx, cipher + len, &len);
    cipher_len += len;
    EVP_CIPHER_CTX_free(ctx);
    printf("[AES] encrypted %d bytes\n", plain_len);
    return cipher_len;
}

int aes_decrypt(unsigned char *cipher, int cipher_len, unsigned char *plain) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, KEY, IV);
    int len, plain_len = 0;
    EVP_DecryptUpdate(ctx, plain, &len, cipher, cipher_len);
    plain_len = len;
    EVP_DecryptFinal_ex(ctx, plain + len, &len);
    plain_len += len;
    EVP_CIPHER_CTX_free(ctx);
    printf("[AES] decrypted %d bytes\n", cipher_len);
    return plain_len;
}

int authenticate(const char *user, const char *pass, role_t *role) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return 0;
    char u[50], p[50], r[10];
    while (fscanf(f, "%49s %49s %9s", u, p, r) == 3)
        if (strcmp(user, u) == 0 && strcmp(pass, p) == 0) {

            // role
            if (strcmp(r, "Top") == 0) *role = TOP;
            else if (strcmp(r, "Medium") == 0) *role = MEDIUM;
            else *role = ENTRY;
            
            fclose(f);
            return 1;
        }
    fclose(f);
    return 0;
}

void process_command(char *cmd, char *arg, role_t user_level, char *response) {
    if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "cat") == 0) {
        sprintf(response, "Command '%s' allowed file reading", cmd);
    }
     else if (strcmp(cmd, "edit") == 0 || strcmp(cmd, "copy") == 0) {
        if (user_level < MEDIUM) {
            sprintf(response, "Access denied need to be Medium level to edit or copy files.");
        } else {
            sprintf(response, "Command '%s' allowed.", cmd);
        }
    }
    else if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "upload") == 0 || strcmp(cmd, "download") == 0) {
        if (user_level != TOP) {
            sprintf(response, "Access denied top level can delete, upload or download.");
        } else {
            sprintf(response, "Command '%s' allowed", cmd);
        }
    }
    else {
        sprintf(response, "unrecognized command try ls, cat, edit, delete, upload, download or copy.");
    }
}

void *client_handler(void *arg) {
    int client_sock = (int)(intptr_t)arg;
    unsigned long thread_id = (unsigned long)pthread_self();
    char buffer[BUFFER_SIZE] = {0};
    unsigned char encrypted[BUFFER_SIZE];

    if (!authenticate(user, pass)) {
        char fail[] = "AUTH_FAIL";
        int enc_len = aes_encrypt((unsigned char*)fail, strlen(fail), encrypted);
        send(client_sock, encrypted, enc_len, 0);
        printf("[thread %lu] authentication failed for %s\n", thread_id, user);
        close(client_sock);
        return NULL;
    }

    printf("[thread %lu] [+] Authenticated: %s\n", thread_id, user);
    char ok[] = "AUTH_OK";
    int enc_len = aes_encrypt((unsigned char*)ok, strlen(ok), encrypted);
    send(client_sock, encrypted, enc_len, 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int msg_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (msg_len <= 0) {
            break;
        }
        dec_len = aes_decrypt((unsigned char*)buffer, msg_len, (unsigned char*)buffer);
        buffer[dec_len] = '\0';
        printf("[thread %lu] %s: %s\n", thread_id, user, buffer);

       
        char reply[BUFFER_SIZE];
        snprintf(reply, sizeof(reply), "Server received: %.1006s", buffer);
        enc_len = aes_encrypt((unsigned char*)reply, strlen(reply), encrypted);
        send(client_sock, encrypted, enc_len, 0);
    }
    printf("[thread %lu] [-] %s disconnected\n", thread_id, user);
    close(client_sock);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in addr;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
        if (client_sock < 0) continue;
        printf("[+] New connection\n");
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, (void*)(intptr_t)client_sock);
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}