/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   server_headers.h
 * Author: nishanth
 *
 * Created on 28 September, 2019, 12:17 PM
 */

#ifndef SERVER_HEADERS_H
#define SERVER_HEADERS_H

#ifdef __cplusplus
extern "C" {
#endif

    #include <stdio.h>
    #include <stdlib.h>
    #include <sqlite3.h>
    #include <string.h>
    #include <math.h>    
    #include <pthread.h>
    
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include<arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    
    #define BUFFER_SIZE 512
    #define BUFFER_SIZE_NETWORK 512
    #define MAX_CLIENTS 1000
    #define MAX_THREADS 5
    
    sqlite3* db;
    char dbName[] = "dsProject.db";
    int dbRetVal;
    
    void initConnectToDB(void){
        dbRetVal = sqlite3_open(dbName, &db);
        if(dbRetVal!=0){
            fprintf(stderr, "Cannot Open Database: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(1);
        }
        printf("Connection with the Database is successful\n");
    }
    typedef struct sockClient
    {
        int new_sock;
        struct sockaddr_in client;
    }sockClient;

#ifdef __cplusplus
}
#endif

#endif /* SERVER_HEADERS_H */

