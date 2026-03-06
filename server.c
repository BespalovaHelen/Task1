#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "protocol.h"

#define MAX_CLIENTS 1000
#define MAX_CARS 1000

typedef struct {
    char brand[MAX_CAR_NAME];
} Car;

typedef struct {
    int socket;
    struct sockaddr_in address;
    pthread_t thread_id;
} Client;

Car car_database[MAX_CARS];
int car_count = 0;
Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_socket;
int server_running = 1;

void broadcast_message(const char *message, int exclude_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != exclude_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int remove_car(const char *brand) {
    pthread_mutex_lock(&db_mutex);
    for (int i = 0; i < car_count; i++) {
        if (strcmp(car_database[i].brand, brand) == 0) {
            for (int j = i; j < car_count - 1; j++) {
                car_database[j] = car_database[j + 1];
            }
            car_count--;
            pthread_mutex_unlock(&db_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&db_mutex);
    return 0;
}

void add_car(const char *brand) {
    pthread_mutex_lock(&db_mutex);
    if (car_count < MAX_CARS) {
        strncpy(car_database[car_count].brand, brand, MAX_CAR_NAME - 1);
        car_database[car_count].brand[MAX_CAR_NAME - 1] = '\0';
        car_count++;
    }
    pthread_mutex_unlock(&db_mutex);
}

void send_car_list(int client_socket) {
    pthread_mutex_lock(&db_mutex);
    char response[BUFFER_SIZE];
    
    if (car_count == 0) {
        int len = snprintf(response, BUFFER_SIZE, "%s В салоне нет машин\n", PREFIX_INFO);
        send(client_socket, response, len, 0);
    } else {
        int len = snprintf(response, BUFFER_SIZE, "%s Машины в наличии:\n", PREFIX_INFO);
        send(client_socket, response, len, 0);
        
        for (int i = 0; i < car_count; i++) {
            char car_line[BUFFER_SIZE];
            int len = snprintf(car_line, BUFFER_SIZE, "  - %s\n", car_database[i].brand);
            if (len > 0 && len < BUFFER_SIZE) {
                send(client_socket, car_line, len, 0);
            }
        }
    }
    pthread_mutex_unlock(&db_mutex);
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    
    printf("Новый клиент: %s:%d\n", 
           inet_ntoa(client->address.sin_addr), 
           ntohs(client->address.sin_port));
    
    while (server_running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            if (server_running) {
                printf("Клиент отключился: %s:%d\n", 
                       inet_ntoa(client->address.sin_addr), 
                       ntohs(client->address.sin_port));
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        buffer[strcspn(buffer, "\n")] = 0;
        
        printf("От %s:%d: %s\n", 
               inet_ntoa(client->address.sin_addr), 
               ntohs(client->address.sin_port), 
               buffer);
        
        if (strncmp(buffer, REQ_BUY, 3) == 0) {
            char brand[MAX_CAR_NAME];
            if (sscanf(buffer + 4, "%49s", brand) == 1) {
                if (remove_car(brand)) {
                    char resp[BUFFER_SIZE];
                    int len = snprintf(resp, BUFFER_SIZE, "%s Машина '%s' продана. Поздравляем!\n", 
                                     PREFIX_ALERT, brand);
                    send(client->socket, resp, len, 0);
                    
                    len = snprintf(resp, BUFFER_SIZE, "%s Машина '%s' куплена в салоне.\n", 
                                 PREFIX_INFO, brand);
                    broadcast_message(resp, client->socket);
                } else {
                    char resp[BUFFER_SIZE];
                    int len = snprintf(resp, BUFFER_SIZE, "%s Машина '%s' не найдена.\n", 
                                     PREFIX_ERR, brand);
                    send(client->socket, resp, len, 0);
                }
            } else {
                char *msg = PREFIX_ERR " Используйте: BUY <машина>\n";
                send(client->socket, msg, strlen(msg), 0);
            }
        }
        else if (strncmp(buffer, REQ_SELL, 4) == 0) {
            char brand[MAX_CAR_NAME];
            if (sscanf(buffer + 5, "%49s", brand) == 1) {
                add_car(brand);
                
                char resp[BUFFER_SIZE];
                int len = snprintf(resp, BUFFER_SIZE, "%s Машина '%s' добавлена в салон. Спасибо!\n", 
                                 PREFIX_OK, brand);
                send(client->socket, resp, len, 0);
                
                len = snprintf(resp, BUFFER_SIZE, "%s Новая машина '%s' теперь доступна.\n", 
                             PREFIX_INFO, brand);
                broadcast_message(resp, -1);
            } else {
                char *msg = PREFIX_ERR " Используйте: SELL <машина>\n";
                send(client->socket, msg, strlen(msg), 0);
            }
        }
        else if (strcmp(buffer, REQ_CARS) == 0) {
            send_car_list(client->socket);
        }
        else {
            char *msg = PREFIX_ERR " Неизвестная команда. Используйте BUY, SELL или CARS.\n";
            send(client->socket, msg, strlen(msg), 0);
        }
    }
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client->socket) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    close(client->socket);
    free(client);
    return NULL;
}

void shutdown_server(int sig) {
    (void)sig;
    printf("\nОстановка сервера...\n");
    
    server_running = 0;
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        close(clients[i].socket);
    }
    pthread_mutex_unlock(&clients_mutex);
    
    close(server_socket);
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Ошибка: неверный порт. Используйте значение от 1 до 65535\n");
            return 1;
        }
    }
    
    struct sockaddr_in server_address;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Ошибка setsockopt");
        close(server_socket);
        return 1;
    }
    
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Ошибка bind");
        close(server_socket);
        return 1;
    }
    
    if (listen(server_socket, 10) < 0) {
        perror("Ошибка listen");
        close(server_socket);
        return 1;
    }
    
    printf("Сервер автосалона запущен на порту %d\n", port);
    printf("Для остановки нажмите Ctrl+C\n");
    
    signal(SIGINT, shutdown_server);
    signal(SIGTERM, shutdown_server);
    
    while (server_running) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);
        
        if (client_socket < 0) {
            if (server_running) {
                perror("Ошибка accept");
            }
            continue;
        }
        
        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            Client *new_client = (Client*)malloc(sizeof(Client));
            if (new_client == NULL) {
                pthread_mutex_unlock(&clients_mutex);
                close(client_socket);
                continue;
            }
            
            new_client->socket = client_socket;
            new_client->address = client_address;
            
            clients[client_count++] = *new_client;
            
            if (pthread_create(&new_client->thread_id, NULL, handle_client, new_client) != 0) {
                perror("Ошибка создания потока");
                client_count--;
                free(new_client);
                close(client_socket);
                pthread_mutex_unlock(&clients_mutex);
                continue;
            }
            
            pthread_detach(new_client->thread_id);
        } else {
            char *msg = "ОШИБКА: Сервер переполнен. Попробуйте позже.\n";
            send(client_socket, msg, strlen(msg), 0);
            close(client_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    
    close(server_socket);
    return 0;
}
