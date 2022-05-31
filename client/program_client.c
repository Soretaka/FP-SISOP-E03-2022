#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>
#include <termios.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#define PORT 8080
char buffer[1024], cmd[1024];
static const char *ERROR = "error";
static const char *ROOT = "root";
int sock = 0, valread;
int status = 0;
pthread_t thread;

bool rootCek() {
    if(!getuid())return 1;
    else return 0;
}

void *connectionMsg(void *arg) {
    char receive[256];
    memset(receive, 0, sizeof(receive));
    while(1) {
        read(*(int *)arg, receive, 256);
        if (strcmp(receive,"")!=0)printf("%s", receive);
        memset(receive, 0, sizeof(receive));
    }
}

bool ClientAuth(int argc, char const *argv[]) {
    memset(buffer,0,sizeof(buffer));
    if (rootCek())send(sock, ROOT, sizeof(ROOT), 0);
    else if (argc != 5 || strcmp(argv[1], "-u") != 0 || strcmp(argv[3], "-p") != 0) {
        send(sock, ERROR, strlen(ERROR), 0);
        printf("Authentication failed\n");
        return 0;
    }
    else {
        send(sock, argv[2], strlen(argv[2]), 0);
        valread = read(sock, buffer, 1024);
        send(sock, argv[4], strlen(argv[4]), 0);
    }
    memset(buffer, 0, sizeof(buffer));
    return 1;
}

int main(int argc, char const *argv[]) {
    struct sockaddr_in address;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
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
        printf("\nConnection Failed \n");
        return -1;
    }
    //Connection Success
    if (!ClientAuth(argc, argv))return 0;

    memset(buffer,0,sizeof(buffer));
    memset(cmd,0,sizeof(cmd));

    printf("'quit' to exit\n");

    pthread_t messageThread;
    pthread_create(&messageThread, NULL, &connectionMsg, &sock);

    bool redir = 0;

    if (!isatty(fileno(stdin)))redir = 1;

    size_t size;
    char *line = NULL;
    if (redir) {
        while (getline(&line, &size, stdin) != -1) {
            send(sock, line, strlen(line), 0);
            memset(line, 0, sizeof(line));
        }
        printf("\n");
        send(sock, "quit", strlen("quit"), 0);
        valread = read(sock, cmd, sizeof(cmd));
        memset(cmd, 0, sizeof(cmd));
        return 0;
    }

    while (1) {
        scanf("%[^\n]s", buffer);
        getchar();
        send(sock, buffer, strlen(buffer), 0);
        if (strcmp(buffer, "quit")==0) {
            valread = read(sock, cmd, sizeof(cmd));
            return 0;
        }
        memset(buffer, 0, sizeof(buffer));
    }
    return 0;
}
