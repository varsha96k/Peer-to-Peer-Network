#include "data_types_server.h"
#include "server_headers.h"
#include <malloc.h>

#define TRUE 1
#define FALSE 0

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Function Declaration: */
/* connectToDB should be executed only once and prior to the execution of the actual program.
Because server has to have a database to store all the details. */
void ConnectToDB(void) __attribute__((constructor));
int registerRequestHandler(int, char *, FileDetails *, int);
void displaySqlError(int, char *);
int getNumOfChunks(char *, int);

/* The following functions offer the Core Functionality */
ArgumentsAll getFilesListHandler(void);
ArgumentsAll getFilesLocationHandler(char*, char*, int);
ArgumentsAll decodeMessage(char *);
int chunkRegistrationHandler(char *, int, char*, size_t);
int updateActivePeers(char *, int, int);


void ConnectToDB(void){
    initConnectToDB();
}

void displaySqlError(int rc, char *errMsg){
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

int getNumOfChunks(char *fName, int fLength){
    float temp;
    float fileLength;
    fileLength = fLength;
    temp = fileLength*1.0/BUFFER_SIZE;
    return ceil(temp);
}

/*This handles the Register request coming in from the client.
 It populates the server database with the new peer's information*/

int registerRequestHandler(int listeningPort, char *ip, FileDetails *fileDetails, int numOfFiles){ 
    int retVal = 1;
    int i;
    int rc;
    char *query = 0;
    char *fName = 0;
    int fLength = 0;
    char *errMsg = 0;
    size_t numChunks = 0;
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &errMsg);
    displaySqlError(rc, errMsg);
    for(i=0;i<numOfFiles;i++){
        
        fName = fileDetails[i].fileName;
        fLength = fileDetails[i].fileLength;
        // printf("LoopVar:%d \t fName: %s \t fLength: %d \n", i, fName, fLength);
        query = sqlite3_mprintf("INSERT INTO FILES (fileName, fileLength) VALUES('%q',%d);",
                                fName,fLength);

        rc = sqlite3_exec(db,query, NULL, NULL, &errMsg);
        displaySqlError(rc, errMsg);
        sqlite3_free(query);
        
        numChunks = getNumOfChunks(fName, fLength);
        // printf("NumOfChunks: %ld\n", numChunks);
        for(size_t j=0;j<numChunks;j++){
            query = sqlite3_mprintf("INSERT INTO CHUNKS (fileName, chunkIndicator, ipAddress, portNum)"
                    "VALUES ('%q',%10d,'%q',%d);", fName, j+1, ip, listeningPort);
            rc = sqlite3_exec(db,query, NULL, NULL, &errMsg); 

            displaySqlError(rc, errMsg);
            sqlite3_free(query);
        }
        
    }
    query = sqlite3_mprintf("INSERT INTO ACTIVEPEERS(ipAddress, portNum, peerStatus) VALUES('%q',%d,%d);",
                    ip,listeningPort,1);

    rc = sqlite3_exec(db,query, NULL, NULL, &errMsg);
    displaySqlError(rc, errMsg);
    
    rc = sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &errMsg);
    displaySqlError(rc, errMsg);
    return retVal;
}

/*To get the list of all the files present in the Server*/
ArgumentsAll getFilesListHandler(){
    ArgumentsAll a;
    FileDetails *fileDetails;
    char *query = "SELECT fileName, fileLength,(SELECT COUNT(*) FROM FILES) As rowCount FROM FILES;";
    sqlite3_stmt *stmt;
    int rc;
    rc = sqlite3_prepare(db, query, -1, &stmt, 0);
    
    a.numOfFiles=0;
    int i=0;
    char *temp;
    while(sqlite3_step(stmt)==SQLITE_ROW){
        if(a.numOfFiles==0){
            a.numOfFiles = sqlite3_column_int(stmt, 2);   
            a.fileDetails = malloc(a.numOfFiles*sizeof(FileDetails));
        }
        a.fileDetails[i].fileName = malloc(strlen(sqlite3_column_text(stmt, 0)));
        strcpy(a.fileDetails[i].fileName, sqlite3_column_text(stmt, 0));
        a.fileDetails[i].fileLength = sqlite3_column_int(stmt, 1);
        i++;
    }
    return a;   
}

/* to get the list of all the chunks and the corresponding PortNumbers/IPaddress
 * for a given fileName */
ArgumentsAll getFilesLocationHandler(char* fName, char *ip, int peerPortNum){
    ArgumentsAll a;
    FileDetails *fileDetails;
    char *query = sqlite3_mprintf("SELECT A.rowCount, B.fileName, B.chunkIndicator, B.ipAddress, B.portNum FROM ((SELECT COUNT(*) rowCount FROM CHUNKS CH INNER JOIN ACTIVEPEERS AP ON AP.ipAddress=CH.ipAddress AND AP.portNum=CH.portNum AND AP.peerStatus=1 AND AP.peerStatus NOT NULL WHERE fileName='%q' AND chunkIndicator NOT IN (SELECT chunkIndicator FROM chunks WHERE fileName='%q' AND ipAddress='%q' AND portNum=%d) ) A LEFT JOIN (SELECT CH.fileName, CH.chunkIndicator, CH.ipAddress, CH.portNum,AP.peerStatus FROM CHUNKS CH INNER JOIN ACTIVEPEERS AP ON AP.ipAddress=CH.ipAddress AND AP.portNum=CH.portNum AND AP.peerStatus=1 AND AP.peerStatus NOT NULL WHERE fileName='%q' AND chunkIndicator NOT IN (SELECT chunkIndicator FROM chunks WHERE fileName='%q' AND ipAddress='%q' AND portNum=%d) ) B ON 1);", fName, fName, ip, peerPortNum, fName, fName, ip, peerPortNum);
    printf("QUERY: %s\n", query);
    sqlite3_stmt *stmt;
    int rc;
    rc = sqlite3_prepare(db, query, -1, &stmt, 0);
    
    a.numOfFiles=0;
    int i=0;
    char *temp;

    while(sqlite3_step(stmt)==SQLITE_ROW){
        if(a.numOfFiles==0){
            a.numOfFiles = sqlite3_column_int(stmt, 0);
            a.fileLocationDetails = malloc(a.numOfFiles*sizeof(FileLocationDetails));
        }
        if(a.numOfFiles>0){
            a.fileLocationDetails[i].fileName = malloc(strlen(sqlite3_column_text(stmt, 1))+1);
            strcpy(a.fileLocationDetails[i].fileName, sqlite3_column_text(stmt, 1));
            a.fileLocationDetails[i].chunkIndicator = sqlite3_column_int(stmt, 2);
            a.fileLocationDetails[i].ip = malloc(strlen(sqlite3_column_text(stmt, 3))+1);
            strcpy(a.fileLocationDetails[i].ip, sqlite3_column_text(stmt, 3));
            a.fileLocationDetails[i].portNum = sqlite3_column_int(stmt, 4);
            i++;
        }
    }
    return a;
}

/* Decode the message passed by the Peer and interpret the respective handler to be called */
ArgumentsAll decodeMessage(char *encodedMessage){
    ArgumentsAll a;
    char delim[] = ",";
    a.listeningPort = atoi(strtok(encodedMessage, delim));
    a.handler = atoi(strtok(NULL, delim));
    char *token;
    char *decodeTemp;
    switch(a.handler){
        case 1:{
            // registerRequestHandler 
            a.numOfFiles = atoi(strtok(NULL, delim));
            a.fileDetails = malloc(a.numOfFiles*sizeof(FileDetails));
            // splitMessage = strtok(NULL, delim);
            for(int i=0; i<a.numOfFiles;i++){
                token = strtok(NULL, delim);
                a.fileDetails[i].fileName = malloc(strlen(token)+1);
                strcpy(a.fileDetails[i].fileName, token);               
                a.fileDetails[i].fileLength = atoi(strtok(NULL, delim));
            }
            break;
        }
        case 2: // get list of all files from server
            break;

        case 3:{    // get list of all chunks from server
            a.numOfFiles=0;
            token = strtok(NULL, delim);
            a.fileDetails = malloc(sizeof(FileDetails));
            a.fileDetails[0].fileName = malloc(strlen(token)+1);
            strcpy(a.fileDetails[0].fileName, token);
            break;
        }
        case 4:{    // chunk registration to server
            a.numOfFiles = 0;
            a.fileLocationDetails = malloc(sizeof(FileLocationDetails));
            token = strtok(NULL, delim);
            a.fileLocationDetails[0].fileName = malloc(strlen(token)+1);
            strcpy(a.fileLocationDetails[0].fileName, token);
            a.fileLocationDetails[0].chunkIndicator = atoi(strtok(NULL, delim));
            break;
        }
        case 5:{    // update Active Peer Status
            a.numOfFiles = 0;
            a.fileLocationDetails = malloc(sizeof(FileLocationDetails));
            token = strtok(NULL, delim);
            a.fileLocationDetails[0].ip = malloc(strlen(token)+1);
            strcpy(a.fileLocationDetails[0].ip, token);
            a.fileLocationDetails[0].portNum = atoi(strtok(NULL, delim));
            a.peerStatus = atoi(strtok(NULL, delim));
        }
        default:
            printf("Cannot Decode Message sent by the Peer!\n");
    }
    return a;
}

/*This is a handler to cater to chunkDownload Success for the client. It updates the database with the new chunk for the client*/
int chunkRegistrationHandler(char *ip, int portNum, char* fileName, size_t chunkIndicator){
    int retVal = 1;
    int rc;
    char *query = 0;
    char *errMsg = 0;
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &errMsg);

    if(rc==SQLITE_OK){
        query = sqlite3_mprintf("INSERT INTO CHUNKS (fileName, chunkIndicator, ipAddress, portNum) VALUES ('%q', %10d, '%q', %d)", fileName, (int)chunkIndicator, ip, portNum);
        if((rc=sqlite3_exec(db, query, NULL, NULL, &errMsg))==SQLITE_OK){
            sqlite3_free(query);
        }
        else{
            displaySqlError(rc, errMsg);
            retVal = 0;
        }

        if((rc = sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &errMsg))!=SQLITE_OK){
            displaySqlError(rc, errMsg);
            retVal = 0;
        }
    }
    else{
        displaySqlError(rc, errMsg);
        retVal = 0;
    }
    return retVal;
}

/*Update the Active Peer Status*/
int updateActivePeers(char *ip, int portNum, int peerStatus){     
    int retVal = 1;
    int rc;
    char *query = 0;
    char *errMsg = 0;
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &errMsg);

    if(rc==SQLITE_OK){
        query = sqlite3_mprintf("UPDATE ACTIVEPEERS SET peerStatus = %d WHERE ipAddress = '%q' AND portNum = %d", peerStatus, ip, portNum);
        if((rc=sqlite3_exec(db, query, NULL, NULL, &errMsg))==SQLITE_OK){
            sqlite3_free(query);
        }
        else{
            displaySqlError(rc, errMsg);
            retVal = 0;
        }

        if((rc = sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, &errMsg))!=SQLITE_OK){
            displaySqlError(rc, errMsg);
            retVal = 0;
        }
    }
    else{
        displaySqlError(rc, errMsg);
        retVal = 0;
    }
    return retVal;
}

// this is a wrapper written on top of the send function
// adds a header message of the form "<head>messagelength</head>"
void sendMessage(int new_sock, char* serverResponse){
    int length = strlen(serverResponse);
    char messageLen[100];
    sprintf(messageLen, "<head>%d</head>", length);
    // printf("SERVER::: %s\n", messageLen);
    int totalLength = strlen(messageLen)+length;
    char *newMessage = malloc(totalLength+1);
    strcpy(newMessage, messageLen);
    strcat(newMessage, serverResponse);
    newMessage[totalLength] = '\0';
    printf("SENDING MESSAGE: %s\n", newMessage);
    send(new_sock, newMessage, strlen(newMessage), 0);
    free(newMessage);         
}

// receiveMessage wrapper is written on-top of the recv command
// recv just receives the data from an input stream and is highly unstable with multiple send simultaneous requests
// this function uses an additional header message to read through the messages
char *receiveMessage(int socket, char *buffer){
    char *completeMessage;
    int msgLen = 0;
    msgLen = recv(socket, buffer, BUFFER_SIZE_NETWORK, 0);

    if(msgLen<0){
        return NULL;
    }
    buffer[msgLen] = '\0';
    int v = 0;
    char headerMessage[100];
    int headerVar = 0;
    if( (buffer[0]=='<' && buffer[1]=='h') && ((buffer[2]=='e' && buffer[3]=='a') && (buffer[4]=='d' && buffer[5]=='>')) ){
        v = 6;
        while( (buffer[v]!='<' && buffer[v+1]!='\\') && ((buffer[v+2]!='h' && buffer[v+3]!='e') && ((buffer[v+4]!='a' && buffer[v+5]!='d') && buffer[v+6]!='>')) ){
            headerMessage[headerVar++] = buffer[v++];
            printf("%c", headerMessage[headerVar]);
        }
        headerMessage[headerVar] = '\0';    
        v = v+7;
    }

    int completeMessageLen = atoi(headerMessage);
    completeMessage = malloc(completeMessageLen*sizeof(char)+1);
    memset(completeMessage, 0, completeMessageLen);
    int completeMessageVar = 0;

    while(v<msgLen){
        completeMessage[completeMessageVar++] = buffer[v];
        v++;
    }
    while((completeMessageLen-completeMessageVar)>0){
        int i=0;
        msgLen = recv(socket, buffer, BUFFER_SIZE_NETWORK, 0);
        if(msgLen<0)
            break;
        while(i<msgLen){
            completeMessage[completeMessageVar] = buffer[i];
            i++;
            completeMessageVar++;
        }
    }
    completeMessage[completeMessageVar] = '\0';
    printf("RECEIVED MESSAGE: %s\n", completeMessage);
    return completeMessage;
}

// threaded execution of the server to handle multiple requests from the client
void * threadExecution(void *arg){
    sockClient s = *((sockClient *)arg);
    int new_sock = s.new_sock;
    struct sockaddr_in client = s.client;
    char* peerIp = (char *)inet_ntoa(client.sin_addr);
    int peerPortNum = 0;
    int msgLen = 0;
    char buffer[BUFFER_SIZE_NETWORK+1];
    
    while(1)
    {
        pthread_mutex_lock(&lock);
        char *receivedMessage = receiveMessage(new_sock, buffer);
        pthread_mutex_unlock(&lock);
        // if no message is received, then break out of the loop
        if(receivedMessage==NULL){
            break;
        }
        else{
            // if there is a message, then decode the message and call the respective handler
            char *serverResponse;
            int responseSize = 0;
            ArgumentsAll a = decodeMessage(receivedMessage);
            free(receivedMessage);
            peerPortNum = a.listeningPort;

            switch(a.handler){
                case 1:{
                    // registerRequestHandler
                    FileDetails f[a.numOfFiles];
                    for(int i=0;i<a.numOfFiles;i++){
                        f[i].fileName = malloc(strlen(a.fileDetails[i].fileName)+1);
                        strcpy(f[i].fileName, a.fileDetails[i].fileName);
                        f[i].fileLength = a.fileDetails[i].fileLength;
                    }

                    int registerRequestResponse = registerRequestHandler(peerPortNum, peerIp, f, a.numOfFiles);
                    serverResponse = malloc(sizeof(int));
                    sprintf(serverResponse, "%d", registerRequestResponse);
                    if(a.numOfFiles>0)
                        free(a.fileDetails);
                    break;
                }
                case 2:{
                    // update the peer status as ACTIVE and get the list of Files in the database 
                    updateActivePeers(peerIp, peerPortNum, 1);
                    ArgumentsAll responseArg = getFilesListHandler();
                    responseSize = 0;

                    char *temp;
                    int tempSize = 0;
                    char *intermediateResultStr;
                    int prevLength = 0;
                    for(int i=0;i<responseArg.numOfFiles;i++){
                        tempSize = 0;
                        tempSize = snprintf(NULL, 0, "%s,%d", responseArg.fileDetails[i].fileName, responseArg.fileDetails[i].fileLength)+1;
                        temp = malloc(tempSize);
                        memset(temp, 0, tempSize);
                        sprintf(temp, "%s,%d", responseArg.fileDetails[i].fileName, responseArg.fileDetails[i].fileLength);

                        if(i==0){
                            prevLength = 0;
                            intermediateResultStr = calloc(tempSize, sizeof(char));
                        }
                        else{
                            prevLength = strlen(intermediateResultStr);
                            intermediateResultStr = (char *)realloc(intermediateResultStr, prevLength+tempSize+1);// Unpredicatable beheviour                                
                        }

                        strcat(intermediateResultStr, temp);

                        if(i!=responseArg.numOfFiles-1){
                            strcat(intermediateResultStr, ",");
                        }
                        free(temp);
                        free(responseArg.fileDetails[i].fileName);
                    }
                    if(responseArg.numOfFiles>0)
                        free(responseArg.fileDetails);

                    responseSize = snprintf(NULL, 0, "%d,%d,%s", a.handler, responseArg.numOfFiles, intermediateResultStr)+1;
                    serverResponse = malloc(responseSize);
                    sprintf(serverResponse, "%d,%d,%s", a.handler, responseArg.numOfFiles, intermediateResultStr);
                    free(intermediateResultStr);
                    break;
                }
                case 3:{
                    // update the peer status as ACTIVE and get the chunks details in the database 
                    updateActivePeers(peerIp, peerPortNum, 1);
                    ArgumentsAll responseArg = getFilesLocationHandler(a.fileDetails[0].fileName, peerIp, peerPortNum);
                    responseSize = 0;                    
                    char *temp;
                    size_t tempSize = 0;
                    char *intermediateResultStr;
                    size_t prevLength = 0;

                    
                    for(int i=0;i<responseArg.numOfFiles;i++){
                        tempSize = 0;
                        tempSize = snprintf(NULL, 0, "%s,%d,%s,%d", responseArg.fileLocationDetails[i].fileName, responseArg.fileLocationDetails[i].chunkIndicator,
                            responseArg.fileLocationDetails[i].ip, responseArg.fileLocationDetails[i].portNum)+1;
                        temp = malloc(tempSize);
                        
                        sprintf(temp, "%s,%d,%s,%d", responseArg.fileLocationDetails[i].fileName, responseArg.fileLocationDetails[i].chunkIndicator,
                            responseArg.fileLocationDetails[i].ip, responseArg.fileLocationDetails[i].portNum);
                        
                        if(i==0){
                            prevLength = 0;
                            intermediateResultStr = calloc(tempSize, sizeof(char));
                        }
                        else{
                            prevLength = strlen(intermediateResultStr);
                            intermediateResultStr = (char *)realloc(intermediateResultStr, prevLength+tempSize+1);// Unpredicatable beheviour 
                        }

                        strcat(intermediateResultStr, temp);

                        if(i!=responseArg.numOfFiles-1){
                            strcat(intermediateResultStr, ",");
                        }
                        free(temp);
                        free(responseArg.fileLocationDetails[i].fileName);
                        free(responseArg.fileLocationDetails[i].ip);
                    }
                    if(responseArg.numOfFiles>0)
                        free(responseArg.fileLocationDetails);
                    responseSize = snprintf(NULL, 0, "%d,%d,%s", a.handler, responseArg.numOfFiles, intermediateResultStr)+1;
                    serverResponse = malloc(responseSize);

                    sprintf(serverResponse, "%d,%d,%s", a.handler, responseArg.numOfFiles, intermediateResultStr);
                    free(intermediateResultStr);

                    break;
                }
                case 4:{
                    //chunkRegistrationHandler(char *ip, int portNum, char* fileName, size_t chunkIndicator)
                    updateActivePeers(peerIp, peerPortNum, 1);
                    int chunkRegistrationResponse = chunkRegistrationHandler(peerIp, peerPortNum, a.fileLocationDetails[0].fileName, a.fileLocationDetails[0].chunkIndicator);
                    serverResponse = malloc(sizeof(int));
                    sprintf(serverResponse, "%d", chunkRegistrationResponse);
                    break;
                }
                case 5:{
                    // update Active Peer status
                    serverResponse = malloc(sizeof(int));
                    sprintf(serverResponse, "%d", updateActivePeers(a.fileLocationDetails[0].ip, a.fileLocationDetails[0].portNum, a.peerStatus));
                    free(a.fileLocationDetails[0].ip);
                    free(a.fileLocationDetails);
                }
                
            }
            pthread_mutex_lock(&lock);
            sendMessage(new_sock, serverResponse);
            pthread_mutex_unlock(&lock);
            free(serverResponse);
            close(new_sock);
        }
    }
    printf("threadExecution Exiting\n");
}


int main(int argc, char** argv) {
    int sock; // socket descriptor for the server.
    int new_sock; // socket descriptor for the incoming client.
    
    struct sockaddr_in server; //server details
    struct sockaddr_in client; //used by the server for binding to a particular peer
    int sockaddr_len = sizeof(struct sockaddr_in);
    
    char buffer[BUFFER_SIZE_NETWORK+1];
    
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("server socket error\n");
        exit(-1);
    }
    
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[1])); //command-line argument containing the portNum of the server.
    server.sin_addr.s_addr = INADDR_ANY;
    memset(server.sin_zero, '\0', sizeof(server.sin_zero));
    
    if(bind(sock, (struct sockaddr*)&server, sockaddr_len) == -1){
        perror("binding error\n");
        exit(-1);
    }
    
    if(listen(sock, MAX_CLIENTS)==-1){
        perror("listening error");
        exit(-1);
    }

    pthread_t tid[MAX_THREADS];
    int threadVar = 0;

    while(1){
        if((new_sock=accept(sock,(struct sockaddr*)&client,&sockaddr_len))==-1){
           perror("accepting error");
           exit(-1);
        }
        printf("accepted the connection\n\n");    
    
    sockClient combineSockClient;
    combineSockClient.new_sock = new_sock;
    combineSockClient.client = client;

    // execute the handler functions in parallel
    if(pthread_create(&tid[threadVar], NULL, threadExecution, &combineSockClient)!=0){
        printf("Failed to create thread\n");
    }

    if(threadVar>=50){
        threadVar=0;
        while(threadVar<50){
            pthread_join(tid[threadVar++], NULL);
        }
        threadVar=0;
    }
    printf("Exiting\n");
    }
    close(sock);
    return 0;
}

    







