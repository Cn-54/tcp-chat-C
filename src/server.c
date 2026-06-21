
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ROOM_SIZE 10
#define MAX_CLIENTS 100
#define MAX_ROOMS 100

#define PORT 8080
#define BACKLOG 10

#define MAX_WORDS 8


typedef struct room Room;

typedef struct client {
    char *name;
    int fd;
    Room *room; 
}Client;

typedef struct room {
    char *name;
    Client *clients[ROOM_SIZE];
    Client *owner;
    int num_clients;
}Room;

Client *clients[MAX_CLIENTS];
Room *rooms[MAX_ROOMS];


// code for creating and removing clients
Client *Create_Client(char *name, int fd){
    Client *c = malloc(sizeof(Client));
    c->name = strdup(name);
    c->fd = fd;
    c->room = NULL;
    return c;
}

void Destroy_Client(Client *c){
    free(c);
}

int Add_Client(Client *c){
    for(size_t i = 0; i < MAX_CLIENTS; i++){
        if(clients[i] == NULL){
            clients[i] = c;
            return 1;
        }
    }
    return 0;
}

int Remove_Client(Client *c){
    for(size_t i = 0; i < MAX_CLIENTS; i++){
        if(clients[i] == c){
            clients[i] = NULL;
            return 1;
        }
    }
    return 0;
}


// code for broadcasting messages
void client_global_broadcast(char *message, Client *sender){
    char formatted[1100];
    int len = snprintf(formatted, sizeof(formatted), "%s: %s", sender->name, message);
    printf("%s\n",formatted);

    for(size_t i = 0; i < MAX_CLIENTS; i++){
        if(clients[i] == NULL) continue;
        if(clients[i] == sender) continue;
        write(clients[i]->fd, formatted, len);
    }
}

void client_room_broadcast(char *message, Client *sender){
    char formatted[1100];
    int len = snprintf(formatted, sizeof(formatted), "%s: %s", sender->name, message);
    printf("%s\n",formatted);

    Room *room = sender->room;
    if(room == NULL){
        char *message = "you are not currently in a room\n join or create one to start messaging\n";
        write(sender->fd, message,strlen(message));
        return;
    }

    for(size_t i = 0; i < ROOM_SIZE; i++){
        if(room->clients[i] == NULL) continue;
        if(room->clients[i] == sender) continue;
        write(room->clients[i]->fd, formatted, len);
    }
}

void server_global_broadcast(char *message){
    printf("%s\n", message);

    for(size_t i = 0; i < MAX_CLIENTS; i++){
        if(clients[i] == NULL) continue;
        write(clients[i]->fd, message, strlen(message));
    }
}

//command code
int tokenize(char *message, char **words, int max_words){
    int count = 0;
    char *saveptr;
    char *tok = strtok_r(message, " \n", &saveptr);

    while(tok != NULL && count < max_words){
        words[count++] = tok;
        tok = strtok_r(NULL, " \n", &saveptr);
    }
    return count;
}

void doWhoami(Client *sender, char **words, int word_count){
    char msg[1100];
    int len = snprintf(msg, sizeof(msg), "%s\n", sender->name);
    write(sender->fd, msg, len);
    printf("%s ran <whoami>\n",sender->name);
}

void dispatch(char *message, Client *sender){
    char copy[1024];
    strncpy(copy, message, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';

    char *words[MAX_WORDS];
    int count = tokenize(copy, words, MAX_WORDS);
    if(count == 0) return;

    if(strcmp(words[0], "/whoami") == 0){
        doWhoami(sender,words,count);
    }
    else {
        client_room_broadcast(message, sender);
    }
}

// client loop
void Listen_Client(Client *c){
    char *welcome = "you are now connected!\n";
    write(c->fd, welcome,strlen(welcome));
    while(1){
        char message[1024] = {0};
        int er = read(c->fd,message,1024-1);
        if(er < 1) break;
        dispatch(message, c);
    }

    char leave_msg[1100];
    snprintf(leave_msg, sizeof(leave_msg), "%s disconnected!\n", c->name);
    server_global_broadcast(leave_msg);
    Remove_Client(c);
    close(c->fd);
}


void *thread_start(void *arg){
    Client *c = (Client *)arg;
    Listen_Client(c);
    return NULL;
}


//initialisation and main loop for client connections
int main(){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    printf("server online...\n");

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
        perror("bind");
        exit(1);
    }


    if(listen(server_fd, BACKLOG) == -1){
        perror("listen");
        exit(1);
    }

    
    while(1){
        int client_fd = accept(server_fd, NULL, NULL);
        if(client_fd == -1){
            perror("accept");
            continue;
        }

        char name[1024] = {0};
        int n = read(client_fd, name, 1024 - 1);
        if(n < 1){
            close(client_fd);
            continue;
        }
        name[n-1] = '\0';

        char join_msg[1100];
        snprintf(join_msg, sizeof(join_msg), "%s conncted!\n", name);
        server_global_broadcast(join_msg);

        Client *c = Create_Client(name, client_fd);
        Add_Client(c);

        pthread_t tid;
        pthread_create(&tid, NULL, thread_start, c);
        pthread_detach(tid);
    }
    

    return 0;
}