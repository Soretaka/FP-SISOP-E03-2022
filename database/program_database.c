#include <stdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/socket.h>
#define SIZE 50
#define STR_SIZE 1024
#define PORT 8080
#define MAX_COMMANDS 128
#define MAX_COMMAND_LENGTH 64
#define MAX_COLUMN 128
#define MAX_COLUMN_LEN 64
#define MAX_TABLE_LEN 64
int server_fd;
int ctrClient = 0;
int opt = 1;
struct sockaddr_in address;
int addrlen = sizeof(address);

// const for currTime string
static const int TIME_SIZE = 30;
// const for log string size
static const int LOG_SIZE = 1000;

static char *logpath = "database.log";
static char *AUTH_ERROR = "Authentication error.\nClosing connection...\n";
static char *PERM_ERROR = "You have no permission to run that command.\n";
static char *CMMD_ERROR = "Error while parsing command.\nPlease make sure the command syntax.\n";
static char *ROOT = "root";
static char *ERROR = "error";
static char *DB_PROG_NAME = "databaseku";
static char *AUTH_DB = "auth";
static char *USER_TABLE = "users";
static char *PERM_TABLE = "permissions";

pthread_t tid[3000];

typedef struct user_t {
    char name[SIZE];
    char pass[SIZE];
    bool isRoot;
} User;

void makeUser(User* user, char *name, char *pass) {
    strcpy(user->name, name);
    strcpy(user->pass, pass);
}

void printUser(User* user) {
    printf("name: %s\npass: %s\n", user->name, user->pass);
}

bool equalUser(User *a, User *b) {
    return (strcmp(a->name, b->name) == 0 && strcmp(a->pass, b->pass) == 0);
}

bool isAlphanum(char c) {
    if (c >= 'A' && c <= 'Z') return 1;
    else if(c >= 'a' && c <= 'z') return 1;
    else if(c >= '0' && c <= '9') return 1;
    else if (c == '*') return 1;
    else if (c == '=') return 1;
    return 0;
}

void splitCommands(const char *source, char dest[MAX_COMMANDS][MAX_COMMAND_LENGTH], int *command_size) {
    int i = 0, j = 0, k = 0;
    while (i < strlen(source)) {
        if (!isAlphanum(source[i])) {
            if (k) {
                dest[j][k++] = '\0';
                j++;
                k = 0;
            }
        }
        else {
            dest[j][k++] = source[i];
        }
        i++;
    }
    if (k) {
        dest[j][k++] = '\0';
        j++;
        k = 0;
    }
    *command_size = j;
}

void createDatabase(char *name) {
    mkdir(DB_PROG_NAME, 0777);
    char buff[256];
    sprintf(buff, "%s/%s", DB_PROG_NAME, name);
    mkdir(buff, 0777);
}

void createTable(char *db, char *tb, char *attr[64], int size, char dt[MAX_COLUMN][32], int dt_size) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "w");
    if (!fptr) return;
    for (int i = 0; i < size; i++) {
        fprintf(fptr, "%s,", attr[i]);
    }
    fclose(fptr);
    // Create table information
    sprintf(buff, "%s/%s/.%s_nya", DB_PROG_NAME, db, tb);
    fptr = fopen(buff, "w");
    if (fptr) {
        fprintf(fptr, "CREATE TABLE %s (", tb);
        for (int i = 0; i < dt_size; i++) {
            fprintf(fptr, "%s %s", attr[i], dt[i]);
            if (i != dt_size-1) fprintf(fptr, ", ");
        }
        fprintf(fptr, ");");
        fclose(fptr);
    }
}

void insertToTable(char *db, char *tb, char *attr_data[64], int size) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "a");
    if (!fptr) return;
    fprintf(fptr, "\n");
    for (int i = 0; i < size; i++) {
        fprintf(fptr, "%s,", attr_data[i]);
    }
    fclose(fptr);
    printf("[Log] INSERT INTO %s.%s (", db, tb);
    for (int i = 0; i < size; i++) {
        printf("%s", attr_data[i]);
        if (i != size-1) printf(", ");
    }
    printf(")\n");
}

bool doesDatabaseExist(const char name[]) {
    char buff[256];
    sprintf(buff, "%s/%s", DB_PROG_NAME, name);
    DIR *dir = opendir(buff);
    if (dir) {
        // Directory exists
        closedir(dir);
        return 1;
    }
    return 0;
}

bool doesTableExist(char *db, char *tb) {
    char filePath[256];
    sprintf(filePath, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    FILE *f = fopen(filePath, "r");
    if (f) {
        // table exists
        fclose(f);
        return 1;
    }
    return 0;
}

void createUser(char *u, char *p) {
    char *attr[64];
    attr[0] = u;
    attr[1] = p;
    insertToTable(AUTH_DB, USER_TABLE, attr, 2);
}

void __createUsersTable() {
    char *attr[64];
    attr[0] = "name";
    attr[1] = "pass";
    if (!doesTableExist(AUTH_DB, USER_TABLE)) {
        char dt[2][32] = {"string", "string"};
        createTable(AUTH_DB, USER_TABLE, attr, 2, dt, 2);
    }
}

void __createPermissionsTable() {
    char *attr[64];
    attr[0] = "database";
    attr[1] = "user";
    if (!doesTableExist(AUTH_DB, PERM_TABLE)) {
        char dt[2][32] = {"string", "string"};
        createTable(AUTH_DB, PERM_TABLE, attr, 2, dt, 2);
    }
}

void AuthInit() {
    if (!doesDatabaseExist(AUTH_DB)) {
        createDatabase(AUTH_DB);
    }
    __createUsersTable();
    __createPermissionsTable();
}

void __readUserTable(User *user, char *line) {
    char u[SIZE];
    char p[SIZE];

    int i = 0;
    int j = 0;
    while (line[i] != ',') {
        u[j++] = line[i++];
    }
    u[j++] = '\0';
    i++;
    strcpy(user->name, u);

    j = 0;
    while (line[i] != ',') {
        p[j++] = line[i++];
    }
    p[j++] = '\0';
    strcpy(user->pass, p);
}

bool NormalUserAuth(User *user) {
    char filePath[256];
    sprintf(filePath, "%s/%s/%s.nya", DB_PROG_NAME, AUTH_DB, USER_TABLE);
    FILE *f = fopen(filePath, "r");

    if (!f) {
        printf("error opening file");
        return false;
    }
    User attempt;
    char temp[256];
    int temp_size = 0;
    char ch;
    while (fscanf(f, "%c", &ch) != EOF) {
        if (ch == '\n') {
            if (temp_size) {
                temp[temp_size++] = '\n';
                temp[temp_size++] = '\0';
                __readUserTable(&attempt, temp);
                if (equalUser(user, &attempt)) {
                    return true;
                }

                temp_size = 0;
            }
        }
        else {
            temp[temp_size++] = ch;
        }
    }
    if (temp_size) {
        temp[temp_size++] = '\n';
        temp[temp_size++] = '\0';
        __readUserTable(&attempt, temp);
        if (equalUser(user, &attempt)) {
            return true;
        }
        temp_size = 0;
    }
    fclose(f);

    return false;
}

bool ClientAuth(User* user) {
    if (strcmp(user->name, ROOT) == 0) {
        user->isRoot = 1;
        return 1;
    }
    else if (NormalUserAuth(user)) {
        user->isRoot = 0;
        return 1;
    }

    return 0;
} 

void MessageSentByDB(int *new_socket, char *msg) {
    send(*new_socket, msg, strlen(msg), 0);
}

void SelectCommand(int *sock, const char *db, const char *tb) {
    char buffer[256],temp[256];
    int temp_size = 0;
    sprintf(buffer, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);

    FILE *fptr = fopen(buffer, "r");
    if (!fptr) return;
    char ch;
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == '\n') {
            if (temp_size) {
                temp[temp_size++] = '\n';
                temp[temp_size++] = '\0';
                MessageSentByDB(sock, temp);
                temp_size = 0;
            }
        }
        else {
            temp[temp_size++] = ch;
        }
    }
    if (temp_size) {
        temp[temp_size++] = '\n';
        temp[temp_size++] = '\0';
        MessageSentByDB(sock, temp);
        temp_size = 0;
    }
    fclose(fptr);
}

void SelectWithWhere(int *sock, const char *db, const char *tb, const char *col, const char *val) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "r");
    if (!fptr) return;
    char printable[STR_SIZE];
    char ch;
    char temp[256],itterator_col[256];
    int temp_size = 0;
    int itterator_col_size = 0;
    bool isInHeader = true;
    int col_number = 1;
    bool found_col = false;
    int itt_col_num = 1;
    bool row_valid = false;

    while (fscanf(fptr, "%c", &ch) != EOF){
        if (ch == '\n') {
            itt_col_num = 1;
            
            if (temp_size) {
                temp[temp_size++] = '\n';
                temp[temp_size++] = '\0';
                if(row_valid || isInHeader)MessageSentByDB(sock, temp);
                printf("CEK SELECT WHERE : temp isinya %s\n", temp);
                temp_size = 0;
            }
            isInHeader = false;
            row_valid = false;
        }
        else {
            if(ch == ','){
                itterator_col[itterator_col_size] = '\0';
                itterator_col_size = 0;
                printf("CEK SELECT WHERE : itt_col = %s dan val = %s\n",itterator_col, val);
                printf("CEK SELECT WHERE : itt_col_num = %d dan col_number = %d\n",itt_col_num, col_number);
                if(itt_col_num == col_number){
                    if(!isInHeader && found_col){
                        if(strcmp(val, itterator_col) == 0){
                            row_valid = true;
                            printf("INI BENAR\n");
                        }else{
                            // printf("INI GAK VALID\n");
                            // row_valid = false;
                        }
                    }
                }
                itt_col_num++;

                if(isInHeader && !found_col){
                    if(strcmp(col, itterator_col) == 0){
                        printf("DAPET kolom nomor %d\n", col_number);
                        found_col = true;

                    }else{
                        col_number++;
                    }
                }
                memset(itterator_col, 0, sizeof(itterator_col));
            }else{
                itterator_col[itterator_col_size++] = ch;
            }
            temp[temp_size++] = ch; 
        }
    }
    if (temp_size) {
        temp[temp_size++] = '\n';
        temp[temp_size++] = '\0';
        if(row_valid)MessageSentByDB(sock, temp);
        printf("CHECK DI TEMP_SIZE : temp %s\n", temp);
        temp_size = 0;
    }
    fclose(fptr);

}

bool ColContainStr(char str[MAX_COLUMN_LEN], const char arr[MAX_COLUMN][MAX_COLUMN_LEN], int arr_size) {
    for (int i = 0; i < arr_size; i++) if (strcmp(str, arr[i]) == 0) return 1;
    return 0;
}

void SelectCommand2(int *sock, const char *db, const char *tb, const char col[MAX_COLUMN][MAX_COLUMN_LEN], int col_size) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "r");
    if (!fptr) return;
    char printable[STR_SIZE];
    char tb_col[MAX_COLUMN_LEN];
    char tb_col_size = 0;
    bool tb_col_reserved[MAX_COLUMN];
    memset(tb_col_reserved, 0, sizeof(tb_col_reserved));
    memset(printable, 0, sizeof(printable));
    int tb_col_number = 0;
    char ch;

    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if (ColContainStr(tb_col, col, col_size)) {
                tb_col_reserved[tb_col_number] = 1;

                //Exception Found
                if(col_size == 1)sprintf(printable, "%16s ", tb_col);
                else sprintf(printable, "%s%16s ", printable, tb_col);
            }
            tb_col[0] = '\0';
            tb_col_size = 0;
            tb_col_number++;
        }
        else if (ch == '\n') {
            // ch ada newline
            strcat(printable, "\n");
            MessageSentByDB(sock, printable);
            printf("%s", printable);
            break;
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    tb_col_number = 0;
    memset(printable, 0, sizeof(printable));

    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if (tb_col_reserved[tb_col_number++]) {
                sprintf(printable, "%s%16s ", printable, tb_col);
            }
            tb_col[0] = '\0';
            tb_col_size = 0;
        }
        else if (ch == '\n') {
            // ch nemu newline
            tb_col_number = 0;
            strcat(printable, "\n");
            MessageSentByDB(sock, printable);
            printf("%s", printable);
            memset(printable, 0, sizeof(printable));
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    strcat(printable, "\n");
    MessageSentByDB(sock, printable);
    printf("%s", printable);
    memset(printable, 0, sizeof(printable));
    fclose(fptr);
}

void SelectCommand4(int *sock, const char *db, const char *tb, const char col[MAX_COLUMN][MAX_COLUMN_LEN], int col_size, const char *col_in_where, const char *val) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    printf("cek isi buff : %s dan kolom di : %s\n", buff, col_in_where);

    FILE *fptr = fopen(buff, "r");
    if (!fptr) return;
    char printable[STR_SIZE],tb_col[MAX_COLUMN_LEN];
    char tb_col_size = 0;
    bool tb_col_reserved[MAX_COLUMN];
    memset(tb_col_reserved, 0, sizeof(tb_col_reserved));
    memset(printable, 0, sizeof(printable));
    int tb_col_number = 0;
    char ch;

    char itt_cell_name[256];
    int itt_cell_name_size = 0;
    int col_number = 1;
    bool col_founded = false;
    
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            itt_cell_name[itt_cell_name_size] = '\0';
            itt_cell_name_size = 0;
            
            if(!col_founded){
                //printf("coba cek kolom dimana : %s\n", col_in_where);
                //printf("cek juga nama cell : %s\n", itt_cell_name);

                if(strcmp(itt_cell_name, col_in_where) == 0){
                    //printf("cell_name dan col_in_where cocok!!!");
                    col_founded = true;
                }else{
                    col_number++;
                }
            }

            if (ColContainStr(tb_col, col, col_size)) {
                tb_col_reserved[tb_col_number] = 1;
                if(col_size == 1)sprintf(printable, "%16s ", tb_col);
                else sprintf(printable, "%s%16s ", printable, tb_col);
                //printf("PRINTABLE after : %s\n", printable);
            }
            tb_col[0] = '\0';
            tb_col_size = 0;
            tb_col_number++;

            memset(itt_cell_name, 0, sizeof(itt_cell_name));
        }
        else if (ch == '\n') {
            // ch nemu newline
            strcat(printable, "\n");
            MessageSentByDB(sock, printable);
            break;
        }
        else {
            tb_col[tb_col_size++] = ch;
            itt_cell_name[itt_cell_name_size++] = ch;
        }
    }
    tb_col_number = 0;
    memset(printable, 0, sizeof(printable));

    int itt_col_num = 1;
    bool is_cell_valid = false;

    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            itt_cell_name[itt_cell_name_size] = '\0';
            itt_cell_name_size = 0;

            if(itt_col_num == col_number){
                if(strcmp(itt_cell_name, val) == 0){
                    //printf("itt_cell_name cocok dengan val!!!\n");
                    is_cell_valid = true;
                }
            }

            if (tb_col_reserved[tb_col_number++]) {
                sprintf(printable, "%s%16s ", printable, tb_col);
            }
            tb_col[0] = '\0';
            tb_col_size = 0;

            itt_col_num++;
            memset(itt_cell_name, 0, sizeof(itt_cell_name));
        }
        else if (ch == '\n') {
            // New line
            itt_col_num = 1;
            tb_col_number = 0;
            strcat(printable, "\n");
            if(is_cell_valid)MessageSentByDB(sock, printable);
            //printf("%s", printable);
            is_cell_valid = false;
            memset(printable, 0, sizeof(printable));
        }
        else {
            tb_col[tb_col_size++] = ch;
            itt_cell_name[itt_cell_name_size++] = ch;
        }
    }
    strcat(printable, "\n");
    if(is_cell_valid)MessageSentByDB(sock, printable);
    //printf("isi dari printable %s", printable);
    memset(printable, 0, sizeof(printable));
    fclose(fptr);
}

int UpdatedInsideTable(int *sock, const char *db, const char *tb, char col[MAX_COLUMN_LEN], char val[]) {
    char buff[256],buff2[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    sprintf(buff2, "%s/%s/%s_new.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "r");
    if (!fptr) return 0;
    FILE *fptr2 = fopen(buff2, "w");
    if (!fptr2) return 0;
    char tb_col[MAX_COLUMN_LEN];
    int tb_col_size = 0;
    int set_col_id = 0;
    int tb_col_number = 0;
    char ch;

    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if (strcmp(tb_col, col) == 0) {
                set_col_id = tb_col_number;
            }
            fprintf(fptr2, "%s,", tb_col);
            tb_col[0] = '\0';
            tb_col_size = 0;
            tb_col_number++;
        }
        else if (ch == '\n') {
            // ch nemu newline
            fprintf(fptr2, "\n");
            break;
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    tb_col_number = 0;

    int aff_row = 0;
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if ((tb_col_number++) == set_col_id) {
                fprintf(fptr2, "%s,", val);
                aff_row++;
            }
            else {
                fprintf(fptr2, "%s,", tb_col);
            }
            tb_col[0] = '\0';
            tb_col_size = 0;
        }
        else if (ch == '\n') {
            // ch nemu newline
            tb_col_number = 0;
            fprintf(fptr2, "\n");
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    fclose(fptr2);
    fclose(fptr);
    remove(buff);
    rename(buff2, buff);
    return aff_row;
}

int UpdatedInsideTable2(int *sock, const char *db, const char *tb, char col[MAX_COLUMN_LEN], char val[], char *old_val, char *col_in_where) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    char buff2[256];
    sprintf(buff2, "%s/%s/%s_new.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "r");
    if (!fptr) return 0;
    FILE *fptr2 = fopen(buff2, "w");
    if (!fptr2) return 0;
    char tb_col[MAX_COLUMN_LEN];
    int tb_col_size = 0;
    int set_col_id = 0;
    int tb_col_number = 0;
    char ch;
    
    char itt_col[256];
    int itt_col_size = 0;
    int col_number = 1;
    bool col_found = false;
    memset(itt_col, 0, sizeof(itt_col));
    
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            itt_col[itt_col_size] = '\0';
            //printf("DEBUG: itt col = %s and col in where = %s\n", itt_col, col_in_where);
            if((strcmp(itt_col, col_in_where) == 0) && !col_found){
                //printf("KETEMU di colnum = %d\n", col_number);
                col_found = true;
            }

            if (strcmp(tb_col, col) == 0) {
                set_col_id = tb_col_number;
            }
            fprintf(fptr2, "%s,", tb_col);
            tb_col[0] = '\0';
            tb_col_size = 0;
            tb_col_number++;

            if(!col_found)col_number++;
            itt_col_size = 0;
            memset(itt_col, 0, sizeof(itt_col));
        }
        else if (ch == '\n') {
            // ch nemu newline
            fprintf(fptr2, "\n");
            break;
        }
        else {
            tb_col[tb_col_size++] = ch;
            itt_col[itt_col_size++] = ch;
        }
    }
    tb_col_number = 0;

    //char itt_col[256];
    itt_col_size = 0;
    int itt_col_num = 1;
    memset(itt_col, 0, sizeof(itt_col));
    char temp_cell[256];
    memset(temp_cell, 0, sizeof(temp_cell));
    int aff_row = 0;
    bool isValid = false;
    char printable[256];
    char printable_nonvalid[256];
    memset(printable, 0, sizeof(printable));
    memset(printable_nonvalid, 0, sizeof(printable_nonvalid));
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            itt_col[itt_col_size] = '\0';
            // printf("DEBUG 2 :: itt_col = %s and old_val = %s\n", itt_col, old_val);
            // printf("DEBUG 3 :: col_number = %d and itt_col_num = %d\n", col_number, itt_col_num);
            // printf("DEBUG 4 :: tb_col_num = %d and set_col_id = %d\n", tb_col_number, set_col_id);

            
            // printf("DEBUG 5 :: temp_cel = %s and val = %s\n\n",temp_cell, val);
            if ((strcmp(old_val, itt_col) == 0) && (col_number == itt_col_num)) {
                // fprintf(fptr2, "%s,", val);
                // aff_row++;
                isValid = true;
            }
            else {
                //fprintf(fptr2, "%s,", tb_col);
            }

            if(((tb_col_number++) == set_col_id)){
                sprintf(temp_cell, "%s,", val);
                strcat(printable, temp_cell);
            }else{
                sprintf(temp_cell, "%s,", tb_col);
                strcat(printable, temp_cell);
            }

            sprintf(temp_cell, "%s,", tb_col);
            strcat(printable_nonvalid, temp_cell);

            tb_col[0] = '\0';
            tb_col_size = 0;

            itt_col_size = 0;
            itt_col_num++;
            memset(itt_col, 0, sizeof(itt_col));
        }
        else if (ch == '\n') {
            // New line
            if(isValid){
                fprintf(fptr2, "%s", printable);
                fprintf(fptr2, "\n");
                aff_row++;
            }else{
                fprintf(fptr2, "%s", printable_nonvalid);
                fprintf(fptr2, "\n");
            }
            
            printf("CEK %s\n\n", printable);
            memset(printable, 0, sizeof(printable));
            memset(printable_nonvalid, 0, sizeof(printable_nonvalid));
            itt_col_num = 1;
            tb_col_number = 0;
            isValid = false;
        }
        else {
            tb_col[tb_col_size++] = ch;
            itt_col[itt_col_size++] = ch;
        }
    }

    if(isValid){
        fprintf(fptr2, "%s", printable);
        aff_row++;
    }
    else fprintf(fptr2, "%s", printable_nonvalid);
    fclose(fptr2);
    fclose(fptr);
    remove(buff);
    rename(buff2, buff);
    return aff_row;
}

int ColumnDrop(int *sock, const char *db, const char *tb, char col[MAX_COLUMN_LEN], char val[]) {
    char buff[256],buff2[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    sprintf(buff2, "%s/%s/%s_new.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "r");
    if (!fptr) return 0;
    FILE *fptr2 = fopen(buff2, "w");
    if (!fptr2) return 0;
    char tb_col[MAX_COLUMN_LEN];
    int tb_col_size = 0;
    int set_col_id = 0;
    int tb_col_number = 0;
    char ch;

    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            printf("DEBUG :: tb_col = %s and col = %s\n", tb_col, col);
            if (strcmp(tb_col, col) == 0) {
                set_col_id = tb_col_number;
                fprintf(fptr2, "%s", val);
            }else{
                fprintf(fptr2, "%s,", tb_col);
            }
            
            tb_col[0] = '\0';
            tb_col_size = 0;
            tb_col_number++;
        }
        else if (ch == '\n') {
            // ch nemu newline
            fprintf(fptr2, "\n");
            break;
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    tb_col_number = 0;
    int aff_row = 0;
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if ((tb_col_number++) == set_col_id) {
                fprintf(fptr2, "%s", val);
                aff_row++;
            }
            else {
                fprintf(fptr2, "%s,", tb_col);
            }
            tb_col[0] = '\0';
            tb_col_size = 0;
        }
        else if (ch == '\n') {
            // New line
            tb_col_number = 0;
            fprintf(fptr2, "\n");
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    fclose(fptr2);
    fclose(fptr);
    remove(buff);
    rename(buff2, buff);
    return aff_row;
}

int RemoveFromTable(char *db, char *tb, char col[], char val[]) {
    char buff[256];
    sprintf(buff, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);
    char buff2[256];
    sprintf(buff2, "%s/%s/%s_new.nya", DB_PROG_NAME, db, tb);
    FILE *fptr = fopen(buff, "r");
    if (!fptr) return 0;
    FILE *fptr2 = fopen(buff2, "w");
    if (!fptr2) return 0;
    char tb_col[MAX_COLUMN_LEN];
    int tb_col_size = 0;
    int set_col_id = 0;
    int tb_col_number = 0;
    char ch;
    bool useWhere = true;
    if (strcmp(col, "$") == 0) useWhere = false;
    
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if (useWhere && strcmp(tb_col, col) == 0) {
                set_col_id = tb_col_number;
            }
            fprintf(fptr2, "%s,", tb_col);
            tb_col[0] = '\0';
            tb_col_size = 0;
            tb_col_number++;
        }
        else if (ch == '\n') {
            // ch nemu newline
            fprintf(fptr2, "\n");
            break;
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    tb_col_number = 0;
    int aff_row = 0;
    char printable[1024];
    memset(printable, 0, sizeof(printable));
    bool isDeleted = false;
    while (fscanf(fptr, "%c", &ch) != EOF) {
        if (ch == ',') {
            tb_col[tb_col_size++] = '\0';
            if (!useWhere) {
                isDeleted = 1;
                if ((tb_col_number++) == set_col_id)
                    aff_row++;
            }
            else {
                if ((tb_col_number++) == set_col_id && strcmp(tb_col, val) == 0) {
                    isDeleted = 1;
                    aff_row++;
                }
            }
            sprintf(printable, "%s%s,", printable, tb_col);
            tb_col[0] = '\0';
            tb_col_size = 0;
        }
        else if (ch == '\n') {
            // ch nemu newline
            tb_col_number = 0;
            strcat(printable, "\n");
            if (!isDeleted) {
                fprintf(fptr2, "%s", printable);
            }
            memset(printable, 0, sizeof(printable));
            isDeleted = 0;
        }
        else {
            tb_col[tb_col_size++] = ch;
        }
    }
    strcat(printable, "\n");
    if (!isDeleted) {
        fprintf(fptr2, "%s", printable);
    }
    memset(printable, 0, sizeof(printable));
    isDeleted = 0;
    fclose(fptr2);
    fclose(fptr);
    remove(buff);
    rename(buff2, buff);
    return aff_row;
}

void grantPermission(char *db, char *us) {
    char *attr[64];
    attr[0] = db;
    attr[1] = us;
    insertToTable(AUTH_DB, PERM_TABLE, attr, 2);
}

void __hasPermissionToDBHelper(char *line, char *db_r, char *us_r) {
    char d[SIZE];
    char u[SIZE];

    int i = 0;
    int j = 0;
    while (line[i] != ',') {
        d[j++] = line[i++];
    }
    d[j++] = '\0';
    i++;
    strcpy(db_r, d);

    j = 0;
    while (line[i] != ',') {
        u[j++] = line[i++];
    }
    u[j++] = '\0';
    strcpy(us_r, u);
}

bool hasPermissionToDB(char *name, char *db) {

    if (strcmp(name, ROOT) == 0) {
        return true;
    }

    char filePath[256];
    sprintf(filePath, "%s/%s/%s.nya", DB_PROG_NAME, AUTH_DB, PERM_TABLE);
    FILE *f = fopen(filePath, "r");

    if (!f) {
        printf("error opening file");
        return false;
    }
    User attempt;
    char temp[256];
    int temp_size = 0;
    char ch;

    char db_read[SIZE];
    char us_read[SIZE];

    while (fscanf(f, "%c", &ch) != EOF) {
        if (ch == '\n') {
            if (temp_size) {
                temp[temp_size++] = '\n';
                temp[temp_size++] = '\0';
                __hasPermissionToDBHelper(temp, db_read, us_read);

                if (strcmp(db_read, db) == 0 && strcmp(us_read, name) == 0) {
                    return true;
                }

                temp_size = 0;
            }
        }
        else {
            temp[temp_size++] = ch;
        }
    }
    if (temp_size) {
        temp[temp_size++] = '\n';
        temp[temp_size++] = '\0';
        __hasPermissionToDBHelper(temp, db_read, us_read);

        if (strcmp(db_read, db) == 0 && strcmp(us_read, name) == 0) {
            return true;
        }

        temp_size = 0;
    }
    fclose(f);

    return false;
}

int __dropDatabaseHelper(const char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;
      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
             continue;

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf) {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode))
                   r2 = __dropDatabaseHelper(buf);
                else
                   r2 = unlink(buf);
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }

   if (!r)
      r = rmdir(path);

   return r;
}

void dropDatabase(char *db) {
    char path[SIZE];
    sprintf(path, "%s/%s", DB_PROG_NAME, db);
    __dropDatabaseHelper(path);
}

void dropTable(char *db, char *tb) {
    char filePath[SIZE];
    sprintf(filePath, "%s/%s/%s.nya", DB_PROG_NAME, db, tb);

    if (remove(filePath) == 0) {
        printf("[Log] Table %s.%s has ben dropped.\n", db, tb);
    }
    else {
        printf("failed to drop table.");
    }
}

void logging(User *user, char *command) {
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);

    char currTime[TIME_SIZE];
    strftime(currTime, TIME_SIZE, "%Y-%m-%d %H:%M:%S", lt);

    char log[LOG_SIZE];
    sprintf(log, "%s:%s:%s", currTime, user->name, command);

    FILE *out = fopen(logpath, "a");
    fprintf(out, "%s\n", log);
    fclose(out);
}

void exportTable(int *sock, char database_name[], char table_name[]) {
    printf("DEBUG: Exporting %s.%s\n", database_name, table_name);
    char path[256];
    sprintf(path, "%s/%s/%s.nya", DB_PROG_NAME, database_name, table_name);
    FILE *fptr = fopen(path, "r");
    if (fptr) {
        char buffer[STR_SIZE];
        char query[STR_SIZE];
        char temp[MAX_COLUMN_LEN]; int temp_size = 0;
        char ch;
        int new_socket = *(int *)sock;
        // Flush query
        memset(query, 0, sizeof(query));

        // Read creation query in special file
        char nyapath[256];
        sprintf(nyapath, "%s/%s/.%s_nya", DB_PROG_NAME, database_name, table_name);
        FILE *header_fptr = fopen(nyapath, "r");
        if (header_fptr) {
            memset(query, 0, sizeof(query));
            int i = 0;
            while (fscanf(header_fptr, "%c", &ch) != EOF) {
                query[i++] = ch;
            }
            query[i++] = '\0';
            sprintf(buffer, "%s\n", query);
            memset(query, 0, sizeof(query));
            MessageSentByDB(&new_socket, buffer);
            fclose(header_fptr);
        }

        
        while (fscanf(fptr, "%c", &ch) != EOF) {
            if (ch == '\n') 
                // New line is reached, stop while loop
                break;
        }

        // Flush query
        memset(query, 0, sizeof(query));

        strcpy(query, "INSERT INTO ");
        strcat(query, table_name);
        strcat(query, " (");


        while (fscanf(fptr, "%c", &ch) != EOF) {
            if (ch == ',') {
                temp[temp_size++] = '\0';
                strcat(query, temp);
                strcat(query, ", ");
                // Reset temp string
                temp[0] = '\0'; temp_size = 0;
            }
            else if (ch == '\n') {
                // Replace ", " to ");" at the end of query string
                query[strlen(query)-1] = ';';
                query[strlen(query)-2] = ')';
                // Post
                sprintf(buffer, "%s\n", query);
                MessageSentByDB(&new_socket, buffer);

                // Flush query
                memset(query, 0, sizeof(query));
                
                // Construct pre-query string
                strcpy(query, "INSERT INTO ");
                strcat(query, table_name);
                strcat(query, " (");
            }
            else temp[temp_size++] = ch;
        }

        // Replace ", " to ");" at the end of query string
        query[strlen(query)-1] = ';';
        query[strlen(query)-2] = ')';
        // Post
        sprintf(buffer, "%s\n", query);
        MessageSentByDB(&new_socket, buffer);
        
        // Flush query
        memset(query, 0, sizeof(query));

        fclose(fptr);
    }
}

void exportDatabase(int *sock, char database_name[]) {
    // Use dirent.h library to list all tables.
    DIR *dp;
    struct dirent *ep;
    
    char path[256];
    sprintf(path, "%s/%s", DB_PROG_NAME, database_name);

    dp = opendir(path);

    if (dp) {
        int new_socket = *(int *)sock;


        char buffer[STR_SIZE];
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "CREATE DATABASE %s;\n", database_name);

        MessageSentByDB(&new_socket, buffer);

        while ((ep = readdir(dp))) {
            // Ignore hidden files
            if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0 || ep->d_name[0] == '.') continue;
            char table_name[64];
            strcpy(table_name, ep->d_name);
            // Remove .nya extension from string table_name
            table_name[strlen(table_name)-4] = '\0';
            exportTable(sock, database_name, table_name);
        }

        close (new_socket);

        closedir(dp);
    }
}

void *client(void *tmp) { 
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int valread, new_socket = *(int *)tmp;
    User current;
    // Auth Current User
    valread = read(new_socket, buffer, 1024);
    // Cek Root
    if (strcmp(buffer, ROOT) == 0) {
        makeUser(&current, buffer, buffer);
    }
    else if (strcmp(buffer, ERROR) == 0) {
        printf("%s", AUTH_ERROR);
        close(new_socket);
        return 0;
    }
    else {
        char name[50];
        char pass[50];

        strcpy(name, buffer);
        memset(buffer, 0, sizeof(buffer));
        MessageSentByDB(&new_socket, "received");
        
        valread = read(new_socket, buffer, 1024);
        strcpy(pass, buffer);

        makeUser(&current, name, pass);
    }
    memset(buffer, 0, sizeof(buffer));
    // Close Unauthorized User
    if (!ClientAuth(&current)) {
        printf("%s", AUTH_ERROR);
        close(new_socket);
        return 0;
    }
    // Authorized Client Start 

    char selectedDatabase[128];
    selectedDatabase[0] = '\0';
    while (true) {
        valread = read(new_socket, buffer, STR_SIZE);

        if (strcmp(buffer, "quit") == 0) {
            printf("Closing client\n");
            MessageSentByDB(&new_socket, "Closing\n");
            close(new_socket);
            return 0;
        }

        char commands[MAX_COMMANDS][MAX_COMMAND_LENGTH];
        int command_size = 0;
        splitCommands(buffer, commands, &command_size);

        if (strcmp(commands[0], "CREATE") == 0) {
            if (strcmp(commands[1], "DATABASE") == 0) {
                if (strlen(commands[2])) {
                    if (doesDatabaseExist(commands[2])) {
                        MessageSentByDB(&new_socket, "Error, database with that name already exists.\n");
                    }
                    else {
                        createDatabase(commands[2]);
                        grantPermission(commands[2], current.name);
                        MessageSentByDB(&new_socket, "Database created.\n");
                        logging(&current, buffer);
                    }
                }
                else {
                    MessageSentByDB(&new_socket, "Syntax Error: CREATE DATABASE [database name]\n");
                }
            }
            else if (strcmp(commands[1], "TABLE") == 0) {
                if (selectedDatabase[0] == '\0') MessageSentByDB(&new_socket, "No database is selected.\n");
                else {
                    // CREATE TABLE name (name int, name int)
                    char *attr[64], dt[MAX_COLUMN][32];
                    int attr_i = 0, dt_size = 0;
                    int i = 0;
                    for (int i = 3; i < command_size; i += 2) {
                        attr[attr_i++] = commands[i];
                        strcpy(dt[dt_size++], commands[i+1]);
                    }

                    if (!doesTableExist(selectedDatabase, commands[2])) {
                        createTable(selectedDatabase, commands[2], attr, attr_i, dt, dt_size);
                        logging(&current, buffer);
                        MessageSentByDB(&new_socket, "Table created.\n");
                    }
                    else {
                        MessageSentByDB(&new_socket, "Sorry, a table with that name was already created in this database.\n");
                    }
                }   
            }
            else if (strcmp(commands[1], "USER") == 0) {
                if (!current.isRoot) {
                    MessageSentByDB(&new_socket, PERM_ERROR);
                }
                else if (current.isRoot) {
                    if (command_size != 6) {
                        MessageSentByDB(&new_socket, CMMD_ERROR);
                    }
                    else {
                        createUser(commands[2], commands[5]);
                        char success[STR_SIZE];
                        sprintf(success, "Successfully created user %s\n", commands[2]);
                        logging(&current, buffer);
                        MessageSentByDB(&new_socket, success);
                    }
                }
            }
            else {
                MessageSentByDB(&new_socket, "Syntax Error: CREATE [What to create]");
            }
        }
        else if(strcmp(commands[0], "USE") == 0) {
            if (doesDatabaseExist(commands[1]) && command_size == 2) {
                if (strcmp(commands[1], AUTH_DB) == 0) {
                    MessageSentByDB(&new_socket, PERM_ERROR);
                }
                else if (hasPermissionToDB(current.name, commands[1])) {
                    strcpy(selectedDatabase, commands[1]);
                    logging(&current, buffer);
                    MessageSentByDB(&new_socket, "Database selected.\n");
                }
                else {
                    MessageSentByDB(&new_socket, PERM_ERROR);
                }
            }
            else MessageSentByDB(&new_socket, "Error, database not found.\n");
        }
        else if (strcmp(commands[0], "INSERT") == 0) {
            if (strcmp(commands[1], "INTO") == 0) {
                if (selectedDatabase[0] == '\0') MessageSentByDB(&new_socket, "No database is selected.\n");
                else {
                    /*
                        This function is not robust yet.
                        Please add validation for:
                        1. When no data is written
                            Ex: 
                            * INSERT INTO user
                            * INSERT INTO user ()
                        2. When number of data is not the same as number of column that table has
                            Ex:
                            * CREATE TABLE user (name string, password string)
                            * INSERT INTO user ('Sayu', '12346', 'Female')
                                This command is invalid since user table has 2 columns.
                    */
                    char *attr[64];
                    int attr_i = 0;
                    for (int i = 3; i < command_size; i++) {
                        attr[attr_i++] = commands[i];
                    }
                    if (attr_i) {
                        insertToTable(selectedDatabase, commands[2], attr, attr_i);
                        logging(&current, buffer);
                        MessageSentByDB(&new_socket, "Data inserted.\n");
                    }
                    else MessageSentByDB(&new_socket, "No value is assigned.\n");
                }
            }
            else MessageSentByDB(&new_socket, "Usage: INSERT INTO [database name] (value1, value2, ...)\n");
        }
        else if (strcmp(commands[0], "SELECT") == 0) {
            if (selectedDatabase[0] == '\0') MessageSentByDB(&new_socket, "No database is selected.\n");
            else {
                if (strcmp(commands[1], "*") == 0) {
                    //SELECT * FROM [table] have 4 commands
                    if(command_size >= 4){
                        if(command_size == 4){
                            SelectCommand(&new_socket, selectedDatabase, commands[3]);
                            logging(&current, buffer);
                        }else if(strcmp(commands[4], "WHERE") == 0){
                            char col[MAX_COLUMN_LEN];
                            char value[MAX_COLUMN_LEN];
                            if(command_size == 6){
                                int i=0;
                                //capture columns name
                                while(commands[5][i] != '='){
                                    col[i] = commands[5][i];
                                    i++;
                                }
                                col[i]='\0';
                                printf("%s\n", col);
                                //skip '='
                                i++;
                                //capture value
                                int j=0;
                                
                                while(commands[5][i] != '\0'){
                                    value[j] = commands[5][i];
                                    i++;
                                    j++;
                                }
                                value[j]='\0';
                                printf("%s\n", value);
                            }else if(command_size == 8){
                                strcpy(col, commands[5]);
                                strcpy(value, commands[7]);
                            }
                            
                            SelectWithWhere(&new_socket, selectedDatabase, commands[3], col, value);
                            logging(&current, buffer);
                        }
                    }else{
                        MessageSentByDB(&new_socket, "Syntax error: SELECT [col1, col2 | *] FROM [table]\n");
                    }
                }
                else {
                    // Stores columns that will be selected
                    char col[MAX_COLUMN][MAX_COLUMN_LEN];
                    int col_size = 0;
                    // Stores table name
                    char tb[MAX_TABLE_LEN];
                    // Stores value in -> WHERE col = 'value'
                    char value[MAX_COLUMN_LEN];
                    // Incremental value
                    int i = 1;
                    bool isValid = false;
                    bool withWhere = false;
                    while(i < command_size) {
                        if (strcmp(commands[i], "FROM") == 0) {
                            // After string "FROM", there must be followed by table name
                            if (i + 1 < command_size) {
                                strcpy(tb, commands[i+1]);
                                isValid = true;
                            }
                            // Exit while scope
                            //i = command_size;
                            break;
                        }
                        else {
                            strcpy(col[col_size++], commands[i]);
                            i++;
                        }
                    }
                    
                    char col_where[MAX_COLUMN_LEN];
                    if(strcmp(commands[i+2], "WHERE") == 0){
                        withWhere = true;
                        int k=0;
                        
                       if(command_size == i+4){
                            //capture columns name
                            while(commands[i+3][k] != '='){
                                col_where[k] = commands[i+3][k];
                                k++;
                            }
                            col_where[k] = '\0';
                            printf("%s\n", col_where);
                            //skip '='
                            k++;
                            //capture value
                            int j=0;

                            while(commands[i+3][k] != '\0'){
                                value[j] = commands[i+3][k];
                                k++;
                                j++;
                            }
                            value[j] = '\0';
                            printf("%s\n", value);
                        }else if(command_size == i+6){
                            strcpy(col_where, commands[i+3]);
                            strcpy(value, commands[i+5]);
                        }
                    }
                    
                    if (!isValid) {
                        MessageSentByDB(&new_socket, "Syntax error: SELECT [col1, col2 | *] FROM [table]\n");
                    }
                    else if(!withWhere){
                        printf("col\n");
                        for (int i = 0; i < col_size; i++) printf("%d. `%s`\n", i, col[i]);
                        SelectCommand2(&new_socket, selectedDatabase, tb, col, col_size);
                        logging(&current, buffer);
                    }
                    
                    if(withWhere && isValid){
                        //printf("CEK col = %s", col_where);
                        SelectCommand4(&new_socket, selectedDatabase, tb, col, col_size, col_where, value);
                        logging(&current, buffer);
                    }
                }
            }
        }
        else if (strcmp(commands[0], "UPDATE") == 0) {
            if (doesTableExist(selectedDatabase, commands[1])) {
                if (strcmp(commands[2], "SET") == 0) {
                    if (command_size == 6 && strcmp(commands[4], "=") == 0) {
                        int affected_row = UpdatedInsideTable(&new_socket, selectedDatabase, commands[1], commands[3], commands[5]);
                        char buff[128];
                        sprintf(buff, "%d rows affected.\n", affected_row);
                        MessageSentByDB(&new_socket, buff);
                        logging(&current, buffer);
                    }
                    if(command_size == 10 && strcmp(commands[8], "=") == 0){
                        int affected_row = UpdatedInsideTable2(&new_socket, selectedDatabase, commands[1], commands[3], commands[5], commands[9], commands[7]);
                        char buff[128];
                        sprintf(buff, "%d rows affected.\n", affected_row);
                        MessageSentByDB(&new_socket, buff);
                        logging(&current, buffer);
                    }
                    else MessageSentByDB(&new_socket, "Syntax error: UPDATE [table name] SET [col] = [value]\n");
                }
                else MessageSentByDB(&new_socket, "Syntax error: UPDATE [table name] SET [col] = [value]\n");
            }
            else MessageSentByDB(&new_socket, "Table not found.\n");
        }
        else if (strcmp(commands[0], "DELETE") == 0) {
            if (strcmp(commands[1], "FROM") == 0) {
                if (selectedDatabase[0] == '\0') MessageSentByDB(&new_socket, "No database is selected.\n");
                else {
                    if (doesTableExist(selectedDatabase, commands[2])) {
                        if (command_size == 7 && strcmp(commands[3], "WHERE") == 0 && strcmp(commands[5], "=") == 0) {
                            // Delete with WHERE
                            int affected_row = RemoveFromTable(selectedDatabase, commands[2], commands[4], commands[6]);
                            char buff[128];
                            sprintf(buff, "%d affected row.\n", affected_row);
                            MessageSentByDB(&new_socket, buff);
                        }
                        else {
                            // Delete without where
                            int affected_row = RemoveFromTable(selectedDatabase, commands[2], "$", "$");
                            char buff[128];
                            sprintf(buff, "%d affected row.\n", affected_row);
                            MessageSentByDB(&new_socket, buff);
                        }
                        logging(&current, buffer);
                    }
                    else {
                        MessageSentByDB(&new_socket, "Table not exist.\n");
                    }
                }
            }
            else MessageSentByDB(&new_socket, "Syntax error: DELETE FROM [table name]\n");
        }
        else if (strcmp(commands[0], "GRANT") == 0) {
            if (command_size != 5) {
                MessageSentByDB(&new_socket, "Syntax Error: GRANT PERMISSION [Database Name] INTO [User name]\n");
            }
            else {
                if (!current.isRoot) {
                    MessageSentByDB(&new_socket, PERM_ERROR);
                }
                else if (current.isRoot) {
                    grantPermission(commands[2], commands[4]);
                    char success[STR_SIZE];
                    sprintf(success, "Granted %s permission to database %s\n", commands[4], commands[2]);
                    MessageSentByDB(&new_socket, success);
                    logging(&current, buffer);
                }
            }
        }
        else if (strcmp(commands[0], "DROP") == 0) {
            if (command_size > 2) {
                if (strcmp(commands[1], "DATABASE") == 0) {
                    if (doesDatabaseExist(commands[2])) {
                        if (hasPermissionToDB(current.name, commands[2])) {
                            dropDatabase(commands[2]);
                            logging(&current, buffer);
                            MessageSentByDB(&new_socket, "Database dropped.\n");
                        }
                        else {
                            MessageSentByDB(&new_socket, PERM_ERROR);
                        }
                    }
                    else {
                        MessageSentByDB(&new_socket, "Cannot found database.\n");
                    }
                }
                else if (strcmp(commands[1], "TABLE") == 0) {
                    if (doesTableExist(selectedDatabase, commands[2])) {
                        dropTable(selectedDatabase, commands[2]);
                        logging(&current, buffer);
                    }
                    else {
                        MessageSentByDB(&new_socket, "Cannot found table\n");
                    }
                }
                else if (strcmp(commands[1], "COLUMN") == 0) {
                    if (command_size != 5) {
                        MessageSentByDB(&new_socket, "Syntax Error: DROP COLUMN [column_name] FROM [table_name]\n");
                    }else{
                        ColumnDrop(&new_socket, selectedDatabase, commands[4], commands[2], "");
                        logging(&current, buffer);
                    }
                }
            }
            else {
                MessageSentByDB(&new_socket, "Syntax Error on DROP query\n");
            }
        }
        else if (command_size == 3 && strcmp(commands[0], "EXPORT") == 0 && strcmp(commands[1], "DATABASE") == 0) {
            // commands[2] stores database name
            if (doesDatabaseExist(commands[2])) {
                exportDatabase(&new_socket, commands[2]);
                logging(&current, buffer);
            }
            else MessageSentByDB(&new_socket, "Database not found.");
        }
        else MessageSentByDB(&new_socket, "Command not found.\n");

        memset(buffer, 0, sizeof(buffer));
        memset(commands, 0, sizeof(commands));
    } 
}

int main(int argc, char const *argv[]) {
  pid_t pid, sid;        // Variabel untuk menyimpan PID

  pid = fork();     // Menyimpan PID dari Child Process

  /* Keluar saat fork gagal
  * (nilai variabel pid < 0) */
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  /* Keluar saat fork berhasil
  * (nilai variabel pid adalah PID dari child process) */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }

//   if ((chdir("/")) < 0) {
//     exit(EXIT_FAILURE);
//   }

//   close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
    
  while(1){
        int new_socket;
      
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }
        if (setsockopt(server_fd, SOL_SOCKET, 
            SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 5) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        AuthInit();

        while(true) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        pthread_create(&(tid[ctrClient]), NULL, &client, &new_socket);
        ctrClient++;
    }
  }
    return 0;
} 
