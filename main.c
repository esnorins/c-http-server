#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <regex.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TEXT_HTML "text/html"
#define TEXT_PLAIN "text/plain"
#define IMAGE_JPEG "image/jpeg"
#define IMAGE_PNG "image/png"
#define ARBITRARY_BINARY_DATA "application/octet-stream"

#define KILOBYTE 1024
#define BUF_SIZE 64*KILOBYTE
#define PORT 8888

const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return TEXT_HTML;
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return TEXT_PLAIN;
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return IMAGE_JPEG;
    } else if (strcasecmp(file_ext, "png") == 0) {
        return IMAGE_PNG;
    } else {
        return ARBITRARY_BINARY_DATA;
    }
}

const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return "";
    }
    return dot + 1;
}

// FIXME: Fix spaces not being decoded...
char *url_decode(const char *src) {
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    for (size_t i = 0; i < src_len; i++) {
        printf("[DEBUG]: Decoding URL route one.\n");
        if (src[(int)i] == '%' && (size_t)src + 2 < src_len) {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        } else {
        printf("[DEBUG]: Decoding URL route two.\n");
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';
    return decoded;
}

void build_http_response(const char *filename, const char *file_ext, char *response, size_t *response_len) {
    printf("[DEBUG]: Building HTTP response.\n");
    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUF_SIZE * sizeof(char));
    printf("[INFO]: Setting header to 200 OK with content type %s.\n", mime_type);
    snprintf(header, BUF_SIZE,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "\r\n",
            mime_type);

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        printf("[INFO]: Sending 404 Not Found response.\n");
        snprintf(response, BUF_SIZE,
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "\r\n"
                "404 Not Found");
        *response_len = strlen(response);
        return;
    }

    // struct stat file_stat;
    // fstat(file_fd, &file_stat);
    // off_t file_size = file_stat.st_size;

    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    size_t bytes_read;
    while ((bytes_read = read(file_fd, response + *response_len, BUF_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }
    free(header);
    close(file_fd);
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUF_SIZE * sizeof(char));

    ssize_t bytes_received = recv(client_fd, buffer, BUF_SIZE, 0);
    printf("[INFO]: Bytes received: %lx\n", bytes_received);
    if (bytes_received > 0) {
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
        regmatch_t matches[2];

        while (regexec(&regex, buffer, 2, matches, 0) == 0) {
            buffer[matches[1].rm_eo] = '\0';
            const char *url_encoded_filename = buffer + matches[1].rm_so;
            char *filename = url_decode(url_encoded_filename);

            char file_ext[32];
            strcpy(file_ext, get_file_extension(filename));

            char *response = (char *)malloc(BUF_SIZE * 2 * sizeof(char));
            size_t response_len;
            build_http_response(filename, file_ext, response, &response_len);

            send(client_fd, response, response_len, 0);
            free(response);
            free(filename);
        }
        regfree(&regex);
    }
    close(client_fd);
    free(buffer);
    free(arg);
    return;
}

int main(void) {
    int server_fd;

    int bind_return;
    int listen_return;

    struct sockaddr_in server_info = {0};
    socklen_t server_info_len = sizeof(server_info);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "[ERR]: Initalizing socket failed.\n");
        return server_fd;
    }

    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(PORT);

    if ((bind_return=bind(server_fd, (struct sockaddr *)&server_info, server_info_len)) < 0) { 
        fprintf(stderr, "[ERR]: Binding socket to port %d failed.\n", PORT);
        return bind_return;
    }

    if ((listen_return=listen(server_fd, 0)) < 0) {
        fprintf(stderr, "[ERR]: Listening for connections failed.\n");
        return listen_return;
    }

    struct sockaddr_in client_info = {0};
    socklen_t client_info_len = sizeof(client_info);

    printf("[INFO]: Listening on port %d.\n", PORT);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        if ((*client_fd=accept(server_fd, (struct sockaddr *)&client_info, &client_info_len)) < 0) {
            fprintf(stderr, "[ERR]: Accepting client connection %d failed...\n", *client_fd);
            return *client_fd;
        }

        pthread_t thread_pid;
        pthread_create(&thread_pid, NULL, handle_client, client_fd);
        pthread_detach(thread_pid);
    }

    return 0;
}
