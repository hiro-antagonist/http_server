#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 4096

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';

    // Check for POST request
    if (strncmp(buffer, "POST", 4) == 0) {
        char *content_length_str = strstr(buffer, "Content-Length: ");
        if (content_length_str) {
            int content_length = atoi(content_length_str + 16);

            // Find the start of the file data
            char *file_data = strstr(buffer, "\r\n\r\n");
            if (file_data) {
                file_data += 4; // Skip the "\r\n\r\n"
                int header_size = file_data - buffer;
                int file_size = bytes_received - header_size;

                // Create and open the file with a unique name
                FILE *file = fopen("uploaded_file", "wb");
                if (!file) {
                    perror("fopen");
                    close(client_socket);
                    return;
                }

                // Write the initial part of the file
                fwrite(file_data, 1, file_size, file);

                // Read the remaining part of the file if necessary
                int remaining_size = content_length - file_size;
                while (remaining_size > 0) {
                    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
                    if (bytes_received < 0) {
                        perror("recv");
                        fclose(file);
                        close(client_socket);
                        return;
                    }
                    fwrite(buffer, 1, bytes_received, file);
                    remaining_size -= bytes_received;
                }

                fclose(file);

                // Send response to the client
                char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                send(client_socket, response, strlen(response), 0);
            }
        }
    } else {
        // Send a 405 Method Not Allowed response for non-POST requests
        char *response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    handle_client(client_socket);
    return NULL;
}

int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        // Accept client connections
        client_socket = malloc(sizeof(int));//is this needed?
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (*client_socket < 0) {
            perror("accept");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, client_socket) != 0) {
            perror("pthread_create");
            close(*client_socket);
            free(client_socket);
        }
        pthread_detach(thread);  // Detach the thread to handle the client independently
    }

    close(server_socket);
    return 0;
}
