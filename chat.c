#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in address;
    int id;
    char name[32];
} Client;

Client* clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(const char* message, int sender_id) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->id != sender_id) {
            if (send(clients[i]->socket, message, strlen(message), 0) < 0) {
                perror("Send error");
                continue;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    Client* client = (Client*)arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 32];

    // Welcome the client
    snprintf(message, sizeof(message), "Welcome, Client %d!\n", client->id);
    send(client->socket, message, strlen(message), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0) {
            printf("Client %d disconnected.\n", client->id);
            close(client->socket);

            // Remove the client
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (clients[i] && clients[i]->id == client->id) {
                    clients[i] = NULL;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            free(client);
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Client %d: %s\n", client->id, buffer);

        // Broadcast the message
        snprintf(message, sizeof(message), "Client %d: %s", client->id, buffer);
        broadcast_message(message, client->id);
    }

    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    pthread_t tid;

    // Initialize server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        return EXIT_FAILURE;
    }

    printf("Chat server started on port %d\n", PORT);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Add client to list
        pthread_mutex_lock(&clients_mutex);
        int i;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i]) {
                Client* new_client = (Client*)malloc(sizeof(Client));
                new_client->socket = client_socket;
                new_client->address = client_addr;
                new_client->id = i + 1;
                clients[i] = new_client;

                pthread_create(&tid, NULL, handle_client, (void*)new_client);
                pthread_detach(tid);

                break;
            }
        }
        if (i == MAX_CLIENTS) {
            printf("Max clients connected. Connection rejected.\n");
            close(client_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    close(server_socket);
    return 0;
}
