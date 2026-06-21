#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

void *listen_for_messages(void *arg){
    int fd = *(int *)arg;
    char buffer[1024];

    while(1){
        int n = read(fd, buffer, sizeof(buffer) - 1);
        if(n < 1){
            printf("Disconnected from server.\n");
            break;
        }
        buffer[n] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}

int main(){
    char username[1024];
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd == -1){
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if(connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
        perror("connect");
        exit(1);
    }

    write(sock_fd, username, strlen(username));

    pthread_t tid;
    pthread_create(&tid, NULL, listen_for_messages, &sock_fd);

    char message[1024];
    while(1){
        fgets(message, sizeof(message), stdin);
        write(sock_fd, message, strlen(message));
    }

    return 0;
}