#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 4096
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

void do_ls(char *out) {
    FILE *f = popen("ls -la", "r");
    if (f) { fread(out, 1, BUFFER_SIZE-1, f); pclose(f); }
    else strcpy(out, "ls error");
}

void do_cat(const char *file, char *out) {
    FILE *f = fopen(file, "r");
    if (f) { fread(out, 1, BUFFER_SIZE-1, f); fclose(f); }
    else strcpy(out, "File not found");
}

void do_edit(const char *file, const char *content, char *out) {
    FILE *f = fopen(file, "w");
    if (f) { fprintf(f, "%s", content); fclose(f); snprintf(out, BUFFER_SIZE, "Written %s", file); }
    else snprintf(out, BUFFER_SIZE, "Write failed");
}

void do_copy(const char *src, const char *dst, char *out) {
    FILE *fs = fopen(src, "rb");
    if (!fs) { snprintf(out, BUFFER_SIZE, "Source not found"); return; }
    FILE *fd = fopen(dst, "wb");
    if (!fd) { snprintf(out, BUFFER_SIZE, "Cannot create destination"); fclose(fs); return; }
    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) fwrite(buf, 1, n, fd);
    fclose(fs); fclose(fd);
    snprintf(out, BUFFER_SIZE, "Copied %s to %s", src, dst);
}

void do_delete(const char *file, char *out) {
    snprintf(out, BUFFER_SIZE, remove(file) == 0 ? "Deleted %s" : "Delete failed", file);
}

void send_file(int sock, const char *name) {
    FILE *f = fopen(name, "rb");
    if (!f) {
        char *err = "ERROR: File not found";
        unsigned char enc[BUFFER_SIZE];
        int len = aes_encrypt((unsigned char*)err, strlen(err), enc);
        send(sock, enc, len, 0);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char ok[BUFFER_SIZE];
    snprintf(ok, sizeof(ok), "OK %ld", size);
    unsigned char enc[BUFFER_SIZE];
    int len = aes_encrypt((unsigned char*)ok, strlen(ok), enc);
    send(sock, enc, len, 0);
    unsigned char buf[BUFFER_SIZE], ec[BUFFER_SIZE];
    while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
        len = aes_encrypt(buf, len, ec);
        send(sock, ec, len, 0);
    }
    fclose(f);
}

void recv_file(int sock, const char *name) {
    char buf[BUFFER_SIZE];
    int len = recv(sock, buf, BUFFER_SIZE, 0);
    if (len <= 0) return;
    len = aes_decrypt((unsigned char*)buf, len, (unsigned char*)buf);
    buf[len] = '\0';
    long size;
    if (sscanf(buf, "OK %ld", &size) != 1) return;
    FILE *f = fopen(name, "wb");
    if (!f) return;
    long recvd = 0;
    while (recvd < size) {
        len = recv(sock, buf, BUFFER_SIZE, 0);
        if (len <= 0) break;
        len = aes_decrypt((unsigned char*)buf, len, (unsigned char*)buf);
        fwrite(buf, 1, len, f);
        recvd += len;
    }
    fclose(f);
}

void execute_command(int sock, const char *cmd, const char *arg, role_t role) {
    char reply[BUFFER_SIZE] = {0}, out[BUFFER_SIZE] = {0};
    
    if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "cat") == 0) {
        if (strcmp(cmd, "ls") == 0) do_ls(out);
        else do_cat(arg, out);
        snprintf(reply, BUFFER_SIZE, "%s", out);
    }
    else if (strcmp(cmd, "edit") == 0 || strcmp(cmd, "copy") == 0) {
        if (role < MEDIUM) snprintf(reply, BUFFER_SIZE, "Permission denied");
        else {
            if (strcmp(cmd, "edit") == 0) {
                char file[200], content[BUFFER_SIZE];
                if (sscanf(arg, "%199s %[^\n]", file, content) == 2) do_edit(file, content, out);
                else snprintf(out, BUFFER_SIZE, "edit file");
            } else {
                char src[200], dst[200];
                if (sscanf(arg, "%199s %199s", src, dst) == 2) do_copy(src, dst, out);
                else snprintf(out, BUFFER_SIZE, "copy destination");
            }
            snprintf(reply, BUFFER_SIZE, "%s", out);
        }
    }
    else if (strcmp(cmd, "delete") == 0) {
        if (role != TOP) snprintf(reply, BUFFER_SIZE, "Permission denied not top ");
        else { do_delete(arg, out); snprintf(reply, BUFFER_SIZE, "%s", out); }
    }
    else if (strcmp(cmd, "upload") == 0) {
        if (role != TOP) snprintf(reply, BUFFER_SIZE, "Permission denied not top");
        else { recv_file(sock, arg); snprintf(reply, BUFFER_SIZE, "Uploaded %s", arg); }
    }
    else if (strcmp(cmd, "download") == 0) {
        if (role != TOP) snprintf(reply, BUFFER_SIZE, "Permission denied");
        else { send_file(sock, arg); return; }
    }
    else {
        snprintf(reply, BUFFER_SIZE, "Unknown command");
    }

    unsigned char enc[BUFFER_SIZE];
    int len = aes_encrypt((unsigned char*)reply, strlen(reply), enc);
    send(sock, enc, len, 0);
}

void *client_handler(void *arg) {
    int client_sock = (int)(intptr_t)arg;
    unsigned long thread_id = (unsigned long)pthread_self();
    char buffer[BUFFER_SIZE] = {0};
    unsigned char encrypted[BUFFER_SIZE];
    role_t current_role;

    int auth_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (auth_len <= 0) {
        close(client_sock);
        return NULL;
    }
    int dec_len = aes_decrypt((unsigned char*)buffer, auth_len, (unsigned char*)buffer);
    buffer[dec_len] = '\0';

    char user[50], pass[50];
    sscanf(buffer, "%49[^:]:%49s", user, pass);
    if (!authenticate(user, pass, &current_role)) {
        char fail[] = "AUTH_FAIL";
        int enc_len = aes_encrypt((unsigned char*)fail, strlen(fail), encrypted);
        send(client_sock, encrypted, enc_len, 0);
        printf("[thread %lu] authentication failed for %s\n", thread_id, user);
        close(client_sock);
        return NULL;
    }

    char *role_str = (current_role == TOP) ? "TOP" : (current_role == MEDIUM) ? "MEDIUM" : "ENTRY";
    printf("[thread %lu] Authenticated: %s (%s)\n", thread_id, user, role_str);

    char ok[] = "AUTH_OK";
    int enc_len = aes_encrypt((unsigned char*)ok, strlen(ok), encrypted);
    send(client_sock, encrypted, enc_len, 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int msg_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (msg_len <= 0) break;

        dec_len = aes_decrypt((unsigned char*)buffer, msg_len, (unsigned char*)buffer);
        buffer[dec_len] = '\0';
        printf("[thread %lu] %s: %s\n", thread_id, user, buffer);

        char cmd[50] = {0};
        char arg_str[BUFFER_SIZE] = {0};
        sscanf(buffer, "%49s %1023[^\n]", cmd, arg_str);
        execute_command(client_sock, cmd, arg_str, current_role);
    }

    printf("[thread %lu] %s disconnected\n", thread_id, user);
    close(client_sock);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT) };
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);
    printf("Server listening on port %d\n", PORT);
    while (1) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock < 0) continue;
        printf("[+] New connection\n");
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, (void*)(intptr_t)client_sock);
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}
