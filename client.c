#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "protocol.h"

int sock_fd = 0;
int active = 1;

void sigint_handler(int sig) {
    (void)sig;
    active = 0;
    close(sock_fd);
    printf("\nКлиент остановлен\n");
    exit(0);
}

void *listener(void *arg) {
    (void)arg;
    char msg[BUFFER_SIZE];
    
    while (active) {
        memset(msg, 0, BUFFER_SIZE);
        int received = recv(sock_fd, msg, BUFFER_SIZE - 1, 0);
        
        if (received <= 0) {
            if (active) {
                printf("\nСоединение с сервером потеряно\n");
            }
            active = 0;
            break;
        }
        
        msg[received] = '\0';
        printf("%s", msg);
        fflush(stdout);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Использование: %s <ip_сервера> <порт>\n", argv[0]);
        printf("Пример: %s localhost 7777\n", argv[0]);
        return 1;
    }
    
    char *host = argv[1];
    int port_num = atoi(argv[2]);
    
    struct sockaddr_in srv_addr;
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Ошибка сокета");
        return 1;
    }
    
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port_num);
    
    if (inet_pton(AF_INET, host, &srv_addr.sin_addr) <= 0) {
        perror("Ошибка адреса");
        close(sock_fd);
        return 1;
    }
    
    if (connect(sock_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("Ошибка подключения");
        close(sock_fd);
        return 1;
    }
    
    printf("Подключен к автосалону %s:%d\n", host, port_num);
    printf("Доступные команды:\n");
    printf("  BUY <машина>   - Купить машину\n");
    printf("  SELL <машина>  - Продать машину\n");
    printf("  CARS           - Показать список машин в наличии\n");
    printf("  exit           - Выход\n\n");
    
    signal(SIGINT, sigint_handler);
    
    pthread_t thr;
    if (pthread_create(&thr, NULL, listener, NULL) != 0) {
        perror("Ошибка создания потока");
        close(sock_fd);
        return 1;
    }
    
    char line[BUFFER_SIZE];
    
    while (active) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(line, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) {
            printf("До свидания!\n");
            break;
        }
        
        if (strlen(line) == 0) {
            continue;
        }
        
        if (strncmp(line, REQ_BUY, 3) == 0 || 
            strncmp(line, REQ_SELL, 4) == 0 ||
            strcmp(line, REQ_CARS) == 0) {
            
            char send_buf[BUFFER_SIZE + 1];
            int len = snprintf(send_buf, sizeof(send_buf), "%s\n", line);
            if (len > 0 && (size_t)len < sizeof(send_buf)) {
                send(sock_fd, send_buf, len, 0);
            }
        } else {
            printf("ОШИБКА: Используйте BUY <машина>, SELL <машина> или CARS\n");
        }
    }
    
    active = 0;
    close(sock_fd);
    pthread_join(thr, NULL);
    
    return 0;
}
