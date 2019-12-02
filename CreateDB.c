/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: nishanth
 *
 * Created on 28 September, 2019, 9:54 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
/*
 * 
 */

char* customConcat(const char* s1, const char* s2, const char* s3){
    char* result = malloc(strlen(s1)+strlen(s2)+strlen(s3)+1);
    strcpy(result, s1);
    strcat(result, s2);
    strcat(result, s3);
    return result;
}

int main(int argc, char** argv) {
    sqlite3* db;
    int rc;
    
    rc = sqlite3_open("dsProject.db", &db);
    if(rc!=0){
        fprintf(stderr, "Cannot Open Database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    printf("Create DB Successful.\n");
    char query1[] = "CREATE TABLE FILES("
    "fileName VARCHAR(1024) PRIMARY KEY NOT NULL, "
    "fileLength INT NOT NULL);";
    char query2[] = "CREATE TABLE CHUNKS("
    "fileName VARCHAR(1024) NOT NULL,"
    "chunkIndicator INT NOT NULL,"
    "ipAddress VARCHAR(16) NOT NULL,"
    "portNum INT NOT NULL,"
    "PRIMARY KEY(fileName, chunkIndicator, ipAddress, portNum));";
    char query3[] = "CREATE TABLE ACTIVEPEERS("
    "ipAddress VARCHAR(16) NOT NULL,"
    "portNum INT NOT NULL,"
    "peerStatus BOOLEAN NOT NULL"
    ", PRIMARY KEY(ipAddress, portNum));";
    
    char *errMessage = 0;
    
    char* queries = customConcat(query1, query2, query3);
    rc = sqlite3_exec(db, queries, NULL, ((void *) 0), &errMessage);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL ERROR: %s\n", errMessage);
        sqlite3_free(errMessage);
    }
    else{
        printf("Three Tables are successfully created.\n"
                "1. Files \n"
                "2. Chunks \n"
                "3. peerStatus\n");
    }
    free(queries);
    sqlite3_close(db);
  
    return (EXIT_SUCCESS);
}



