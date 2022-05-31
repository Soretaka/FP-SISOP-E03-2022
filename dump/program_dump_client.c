#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <termios.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#define PORT 8080
#define STR_SIZE 1024
static const char *ROOT = "root";
static const char *ERROR = "error";
int sock = 0, valread;
int status = 0;
pthread_t thread;

bool rootCek() {
    if(!getuid())return 1;
    else return 0;
}

bool ClientAuth(int argc, char const *argv[]) {
    char buffer[STR_SIZE];
    memset(buffer,0,sizeof(buffer));
    if (rootCek()) send(sock, ROOT, sizeof(ROOT), 0);
    else if (argc != 6 || strcmp(argv[1], "-u") != 0 || strcmp(argv[3], "-p") != 0) {
        send(sock, ERROR, strlen(ERROR), 0);
        printf("Authentication failed\n");
        return 0;
    }
    else {
        send(sock, argv[2], strlen(argv[2]), 0);
        valread = read(sock, buffer, STR_SIZE);
        send(sock, argv[4], strlen(argv[4]), 0);
    }
    memset(buffer, 0, sizeof(buffer));
    return 1;
}

void *connectionMsg(void *arg) {
    char sent[STR_SIZE];
    memset(sent, 0, sizeof(sent));
    while(read(*(int *)arg, sent, STR_SIZE) != 0) {
        if (strlen(sent)) {
            printf("%s", sent); // Print fetched data
        }
        memset(sent, 0, sizeof(sent));
    }
}

int main(int argc, char const *argv[]) {
    struct sockaddr_in address;
    struct sockaddr_in  serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed\n");
        return -1;
    }
    //succes connect
    if (!ClientAuth(argc, argv))return 0;
    char sent[STR_SIZE];
    //argv[5]=nama database
    strcpy(sent, "EXPORT DATABASE ");
    strcat(sent, argv[5]);
    strcat(sent, ";");
    pthread_t messageHandler;
    pthread_create(&messageHandler, NULL, &connectionMsg, &sock);
    send(sock, sent, strlen(sent), 0);
    memset(sent, 0, sizeof(sent));
    pthread_join(messageHandler, NULL);
    return 0;
}
