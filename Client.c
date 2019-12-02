#include "peerHeaders.h"
#include "data_types_server.h"

#define ENCODE_MESSAGE_FORMAT "%d,%d,%s"    


char* encodeMessageServer(Message);
void initDirectory(char *, char *);
int fileChunkInit(char *, char*);
void init(char *, char *);
/* The following functions are implemented in the peerHeader file */
int isRegularFile(const char*);
void chunkFile(char *, char *);
char* createChunkDirectory(char *, char *);
void chunkDir(char *, char *);
void removeAllDirectories(char *);
void sendMessage(int, char *);
char* receiveMessage(int, char *);
ArgumentsAll decodeMessageServer(char *);
void * threadedListen(void *);
P2PFileDetails decodeMessageP2P(char *);
char * encodeMessageP2P(P2PMessage);
int updateActivePeers(char *, int, int);

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

char *chunkFold;	// contains all the chunked files
char *p2pFold;		// contains completely downloaded files

// this is to encode the message that is to be passed to the server
char* encodeMessageServer(Message m){
	char *output;
	size_t sizeRequired = snprintf(NULL, 0, ENCODE_MESSAGE_FORMAT, m.listeningPort, m.action, m.arguments)+1;
	output = malloc(sizeRequired*sizeof(char));
	sprintf(output, ENCODE_MESSAGE_FORMAT, m.listeningPort, m.action, m.arguments);
	return output;
}

// to initialize the dataFolder directory to contain ChunkFolder and P2PFolder 
void initDirectory(char *chunkFold, char *p2pFold){
	int dirCheck;
	struct stat st1 = {0};

	if(stat(chunkFold, &st1) == -1) {
    	mkdir(chunkFold, 0777);
	}

	struct stat st2 = {0};
	if(stat(p2pFold, &st2) == -1) {
    	mkdir(p2pFold, 0777);
	}

	return;
}

// to chunk all the files that the Peer wishes to publish
int fileChunkInit(char *chunkFolderPath, char* publishPath){
	int retVal = 1;
	int i=0;
	char delim[] = ",";
	// removeAllDirectories(chunkFolderPath);

	char *token = strtok(publishPath, delim);
	if(token==NULL){
		perror("Publish Path Error - NULL Token Found");
		retVal = 0;
	}
	while(token!=NULL){
		if(isRegularFile(token)){
			// printf("File\n");
			chunkFile(token, chunkFolderPath);
		}
		else{
			// printf("Folder\n");
			chunkDir(token, chunkFolderPath);
		}
		token = strtok(NULL, delim);
	}
	return retVal;
}

// initialize dataFolderPath with the chunkFolder and the P2PFiles Folder
void init(char *dataFolderPath, char *publishPath){
	int chunkFolderPathSize = snprintf(NULL, 0, "%s%s", dataFolderPath,"chunkFolder/")+1;
	int p2pFolderPathSize = snprintf(NULL, 0, "%s%s", dataFolderPath,"p2pFiles/")+1;

	chunkFold = malloc(chunkFolderPathSize*sizeof(char));
	p2pFold = malloc(p2pFolderPathSize*sizeof(char));
	sprintf(chunkFold, "%s%s", dataFolderPath, "chunkFolder/");
	sprintf(p2pFold, "%s%s", dataFolderPath, "p2pFiles/");

	// create directory for storing downloaded chunks and chunkFold for storing all the chunks of the file
	initDirectory(chunkFold, p2pFold);

	fileChunkInit(chunkFold, publishPath);
	return;
}

// to get the FileSize
int getFileSize(char *fullFileName){
	struct stat st;
	stat(fullFileName, &st);
	int size = st.st_size;
	return size;
}

// gets the fileCount within the specified directory
int getFileCountInDir(char *dirPath){
	int fileCount = 0;
	DIR * dirp;
	struct dirent * entry;

	if( (dirp = opendir(dirPath)) == NULL ){
		perror("Could Not Open directory");
		return 0;
	}

	while ((entry = readdir(dirp)) != NULL) {
	    if (entry->d_type == DT_REG) { /* If the entry is a regular file */
	         fileCount++;
	    }
	}
	closedir(dirp);
	return fileCount;
}

// gets the FileDetails - fileName and fileSize and return the argument to the registeerPeer - Caller Function
ArgumentsAll getFileDetails(char *publishPath, int numOfFiles){
	char delim[] = ",";
	int publishPathLen = strlen(publishPath);
	char *publishPathCpy = malloc(publishPathLen+1);
	strcpy(publishPathCpy, publishPath);
	ArgumentsAll a;
	int dirFlag = 0; // set the flag if the publishpath is a directory
	char *token = strtok(publishPathCpy, delim);
	int fileCount = 0;
	int i=0;
	char *temp;
	int tempFileLen; // length of the name of the File

	if(isRegularFile(token)){
		fileCount = numOfFiles;
		a.fileDetails = malloc(fileCount*sizeof(FileDetails));
	}

	else{
		fileCount = getFileCountInDir(token);
		dirFlag = 1;
		a.fileDetails = malloc(fileCount*sizeof(FileDetails));
	}
	a.numOfFiles = fileCount;

	if(dirFlag){
		DIR *dirp;
		struct dirent *entry;
		
		if( (dirp = opendir(publishPath)) == NULL ){
			perror("Could Not Open directory!!");
		}

		while((entry = readdir(dirp)) != NULL) {
	    	if(entry->d_type == DT_REG) { /* If the entry is a regular file */
				tempFileLen = strlen(entry->d_name);
	        	a.fileDetails[i].fileName = malloc(tempFileLen+1);
	        	strcpy(a.fileDetails[i].fileName, entry->d_name);
	        	temp = malloc(publishPathLen+tempFileLen+1);
	        	sprintf(temp, "%s%s", publishPath, entry->d_name);
	        	a.fileDetails[i].fileLength = getFileSize(temp);
	        	free(temp);
	        	i++;
	    	}
		}
		closedir(dirp);
	}

	else{
		while(token!=NULL){
			a.fileDetails[i].fileLength = getFileSize(token);
			a.fileDetails[i].fileName = malloc(strlen(basename(token))+1);
			strcpy(a.fileDetails[i].fileName,basename(token));
			token = strtok(NULL, delim);
			i++;
		}
	}

	return a;
}

// get the list of listeningPort, fileNames, fileSize that Peer Wants to Publish
// registeer the peer with the Server/Tracker along with the list of files that the peer wishes to publish
int registerPeer(int serverSock, int listeningPort, char *publishPath, int numOfFiles){
	char buffer[BUFFER_SIZE_NETWORK];
	int retVal = 0;
	char delim[] = ",";
	ArgumentsAll a = getFileDetails(publishPath, numOfFiles);

	char *temp;
    int tempSize = 0;
    char *intermediateResultStr;
    int prevLength = 0;

    for(int i=0;i<a.numOfFiles;i++){
        tempSize = 0;
        tempSize = snprintf(NULL, 0, "%s,%d", a.fileDetails[i].fileName, a.fileDetails[i].fileLength)+1;
        temp = malloc(tempSize);
        memset(temp, 0, tempSize);
        sprintf(temp, "%s,%d", a.fileDetails[i].fileName, a.fileDetails[i].fileLength);

        if(i==0){
            prevLength = 0;
            intermediateResultStr = calloc(tempSize, sizeof(char));
        }
        else{
            prevLength = strlen(intermediateResultStr);
            intermediateResultStr = (char *)realloc(intermediateResultStr, prevLength+tempSize+1);                                
        }

        strcat(intermediateResultStr, temp);

        if(i!=a.numOfFiles-1){
            strcat(intermediateResultStr, ",");
        }
        free(temp);
        free(a.fileDetails[i].fileName);
    }
    free(a.fileDetails);

    Message m;
    m.listeningPort = listeningPort;
    m.action = 1;
    m.arguments = malloc(strlen(intermediateResultStr)+sizeof(int)+1);
    sprintf(m.arguments, "%d,%s", a.numOfFiles,intermediateResultStr);
    // strcpy(m.arguments, intermediateResultStr);

	free(intermediateResultStr);
	char *encodedStr = encodeMessageServer(m);
	// printf("RegisterPeer Arg: %s\n", encodedStr);
	sendMessage(serverSock, encodedStr);
	free(encodedStr);
	char *receivedMessage = receiveMessage(serverSock, buffer);
	// printf("SERVER RESPONDED: %s\n", receivedMessage);
	free(receivedMessage);
	close(serverSock);
	return retVal;
}

// get all the files present in the server
ArgumentsAll getFileList(int serverSock, int listeningPort){
	char buffer[BUFFER_SIZE_NETWORK];
	Message m;
	m.listeningPort = listeningPort;
	m.action = 2;
	m.arguments = "\0";
	char *encodedStr = encodeMessageServer(m);
	sendMessage(serverSock, encodedStr);

	char *receivedMessage = receiveMessage(serverSock, buffer);
	ArgumentsAll a = decodeMessageServer(receivedMessage);
	
	free(encodedStr);
	free(receivedMessage);
	close(serverSock);
	return a;
}

// gets the File Chunk Details from the tracker
ArgumentsAll getFileChunk(int serverSock, int listeningPort, char *fileName){
	char buffer[BUFFER_SIZE_NETWORK];
	Message m;
	m.listeningPort = listeningPort;
	m.action = 3;
	m.arguments = malloc(strlen(fileName)+1);
	strcpy(m.arguments, fileName);
	char *encodedStr = encodeMessageServer(m);
	sendMessage(serverSock, encodedStr);
	char *receivedMessage = receiveMessage(serverSock,buffer);
	ArgumentsAll a = decodeMessageServer(receivedMessage);
	
	free(m.arguments);
	free(encodedStr);
	free(receivedMessage);
	close(serverSock);
	return a;
}

// compare function for quickSort algorithm - to get rarest chunkIndicator;
int compare(const void *a, const void *b){
	FileLocationDetails *fl1 = (FileLocationDetails *)a;
	FileLocationDetails *fl2 = (FileLocationDetails *)b;
	return (fl1->chunkIndicator-fl2->chunkIndicator);
}

// Returns the Rarest chunkIndicator - NlogN time complexity average;
// sort using quick sort and get the least frequently repeated chunk
FileLocationDetails chooseRarestChunk(ArgumentsAll a, bool *ongoingDownload){
	FileLocationDetails *f = malloc(a.numOfFiles*sizeof(FileLocationDetails));
	for(int i=0;i<a.numOfFiles;i++){
		f[i].chunkIndicator = a.fileLocationDetails[i].chunkIndicator;
		f[i].fileName = malloc(strlen(a.fileLocationDetails[i].fileName));
		strcpy(f[i].fileName, a.fileLocationDetails[i].fileName);
		f[i].ip = malloc(strlen(a.fileLocationDetails[i].ip));
		strcpy(f[i].ip, a.fileLocationDetails[i].ip);
      	  f[i].portNum = a.fileLocationDetails[i].portNum;
	}

	qsort(f, a.numOfFiles, sizeof(FileLocationDetails), compare);
	int minCount=a.numOfFiles+1;
	int res=-1; 
	int currCount=1;
	int index = -1;

    for (int i=1;i<a.numOfFiles;i++){ 
    	if(ongoingDownload[i-1]==true){
    		continue;
    	}
        if(f[i].chunkIndicator == f[i-1].chunkIndicator) 
        	currCount++; 
        else{ 
            if(currCount<minCount){ 
                minCount=currCount; 
                res=f[i-1].chunkIndicator;
                index = i-1; 
            } 
            currCount=1; 
        } 
    } 
   
    if(currCount<minCount) 
    { 
        minCount=currCount; 
        res=f[a.numOfFiles-1].chunkIndicator;
        index=a.numOfFiles-1; 
    } 
    printf("Rarest chunkIndicator: %d\n", res);
    return f[index];
}

// download a chunk from the peer with socket = clientSock
int downloadChunk(int clientSock, char *fileName, int chunkIndicator){
    int file_size;
    char buffer[BUFFER_SIZE_NETWORK];
    FILE *receivedFile;
    struct stat st = {0};
    int msgLen=0;

    // check if the chunkDir is already present; if not, create one;
    char chunkDirPath[strlen(chunkFold)+strlen(fileName)];
    sprintf(chunkDirPath,"%s%s",chunkFold,fileName);
	if (stat(chunkDirPath, &st) == -1) {
	    mkdir(chunkDirPath, 0777);
	}

	// chunkstring length = 9 (fixed);
    char chunkPath[strlen(chunkFold)+strlen(fileName)+9+1];
    sprintf(chunkPath, "%s%s/%09d", chunkFold, fileName, chunkIndicator);
    receivedFile = fopen(chunkPath, "w");
    // Requested Chunk and buffer size are the same
    msgLen = recv(clientSock, &buffer, BUFFER_SIZE_NETWORK, 0);
    // implies that the client quit
    if(msgLen==-1){
    	return -1;
    }
    fwrite(buffer, sizeof(char), msgLen, receivedFile);
    printf("DOWNLOADING FILE: %s \t CHUNK INDICATOR: %09d\n", fileName, chunkIndicator);    
    fclose(receivedFile);    
    close(clientSock);
    return 1;
}

// serve the Chunk to the Requesting Client/Peer
void serveChunk(int clientSock, char *fileName, int chunkIndicator){
	int fd;
    ssize_t sentBytes = 0;
    char fileSize[CHUNK_SIZE];
    struct stat fileStat;
    int offset = 0;
    struct stat st = {0};

	char chunkDirPath[strlen(chunkFold)+strlen(fileName)+1];
    sprintf(chunkDirPath,"%s%s",chunkFold,fileName);    
    if (stat(chunkDirPath, &st) == -1) {
	    mkdir(chunkDirPath, 0777);
	}

    char chunkPath[strlen(chunkFold)+strlen(fileName)+9+1];
    sprintf(chunkPath, "%s%s/%09d", chunkFold, fileName, chunkIndicator);
    // we can handle directory open errors in the future.

    fd = open(chunkPath, O_RDONLY);
    if (fd==-1)
    {
        fprintf(stderr, "Error opening file - %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    /* Get file stats */
    if(fstat(fd, &fileStat)<0)
    {
		fprintf(stderr, "Error fstat --> %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    sprintf(fileSize, "%ld", fileStat.st_size);
    sentBytes = sendfile(clientSock, fd, NULL, BUFFER_SIZE_NETWORK);
    printf("UPLOADING FILE: %s \t CHUNK INDICATOR: %09d\n", fileName, chunkIndicator);
    close(fd);
    close(clientSock);
}


// decode the message sent by Server and call the respective functions to get the desired functionality
// in simple terms, decode the character Buffer passed by the server
ArgumentsAll decodeMessageServer(char *encodedStr){
	ArgumentsAll a;
	char delim[] = ",";
	char *token;
	a.handler = atoi(strtok(encodedStr, delim));
	switch(a.handler){
		// get Files List:
        case 2:{
        	a.numOfFiles = atoi(strtok(NULL, delim));
        	a.fileDetails = malloc(a.numOfFiles*sizeof(FileDetails));

        	for(int i=0;i<a.numOfFiles;i++){
        		token = strtok(NULL, delim);	// fileName
        		a.fileDetails[i].fileName = malloc(strlen(token)+1);
        		strcpy(a.fileDetails[i].fileName, token);
        		a.fileDetails[i].fileLength = atoi(strtok(NULL, delim));
        	}
        	break;

        }
        // get FileChunk Details:
        case 3:{
        	a.numOfFiles = atoi(strtok(NULL, delim));
        	a.fileLocationDetails = malloc(a.numOfFiles*sizeof(FileLocationDetails));
        	for(int i=0;i<a.numOfFiles;i++){
        		token = strtok(NULL, delim);	// fileName
        		a.fileLocationDetails[i].fileName = malloc(strlen(token)+1);
        		strcpy(a.fileLocationDetails[i].fileName, token);
        		a.fileLocationDetails[i].chunkIndicator = atoi(strtok(NULL, delim)); // chunkIndicator
        		token = strtok(NULL, delim); // ip
        		a.fileLocationDetails[i].ip = malloc(strlen(token)+1);
        		strcpy(a.fileLocationDetails[i].ip, token);
        		a.fileLocationDetails[i].portNum = atoi(strtok(NULL, delim)); // portNum
        	}
        	break;
        }

    }
    return a;	
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
        // printf("HEADER Message: \n");
        v = 6;
        while( (buffer[v]!='<' && buffer[v+1]!='\\') && ((buffer[v+2]!='h' && buffer[v+3]!='e') && ((buffer[v+4]!='a' && buffer[v+5]!='d') && buffer[v+6]!='>')) ){
            headerMessage[headerVar++] = buffer[v++];
        }
        headerMessage[headerVar] = '\0';    
        v = v+7;
    }

    int completeMessageLen = atoi(headerMessage);
    // printf("completeMessageLen :%d\n", completeMessageLen);    
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
    return completeMessage;
}

// this is a wrapper written on top of the send function
// adds a header message of the form "<head>messagelength</head>"
void sendMessage(int new_sock, char* serverResponse){
	int length = strlen(serverResponse);
    char messageHeader[100];
    sprintf(messageHeader, "<head>%d</head>", length);
    
    int totalLength = strlen(messageHeader)+length;
    char *newMessage = malloc((totalLength+1)*sizeof(char));
    strcpy(newMessage, messageHeader);
    strcat(newMessage, serverResponse);

    newMessage[totalLength] = '\0';
    send(new_sock, newMessage, strlen(newMessage), 0);
    // printf("@SEND: After SEND-MESSAGE:  %s\n", newMessage);
    if(strlen(newMessage)>0)
    	free(newMessage);
    return;   
}

// register the chunk with the server
void registerChunk(int serverSock, int listeningPort, char *fileName, int chunkIndicator){
	char buffer[BUFFER_SIZE_NETWORK];
	Message m;
	m.action = 4;
	m.listeningPort = listeningPort;
	m.arguments = malloc(strlen(fileName)+sizeof(int)+1);
	sprintf(m.arguments, "%s,%d", fileName, (int)chunkIndicator);
	char * encodedStr = encodeMessageServer(m);
	sendMessage(serverSock, encodedStr);
	receiveMessage(serverSock, buffer);
	free(encodedStr);
	close(serverSock);
	return;
}

int mergeCheck(char *chunkFolderPath, char *baseFileName, char *mergedOutputDir, int totalChunks){
	// ChunkFolderPath - contains the path to directory where the chunkedFolders are placed.
	// baseFileName - contains the fileName of interest - the one which has to be merged after download completion.
	// mergedOutputDir - option for the peer to decide where the downloaded file has to be stored.
	// Let the peer program fixate on the location of the Chunk Folder.
	
	int retVal=0;
	size_t sizeRequired = snprintf(NULL, 0, "%s%s/", chunkFolderPath,baseFileName)+1;

	char *absoluteDirPath = malloc(sizeRequired*sizeof(char));
	sprintf(absoluteDirPath, "%s%s/", chunkFolderPath, baseFileName);

	DIR* dir = opendir(absoluteDirPath);

	if(dir){
		char *command;

		if(isMerge(absoluteDirPath, totalChunks)){
			sizeRequired = snprintf(NULL, 0, "cat \"%s/\"* > \"%s%s\"", absoluteDirPath, mergedOutputDir, baseFileName)+1;

			command = malloc(sizeRequired*sizeof(char));
			sprintf(command, "cat \"%s/\"* > \"%s%s\"", absoluteDirPath, mergedOutputDir, baseFileName);
			system(command);
			retVal=1;
			free(command);
			free(absoluteDirPath);
		}	
	}	
	return retVal;
}

// parallel listening
void * threadedListen(void *arg){
	char buffer[BUFFER_SIZE_NETWORK];

	combinePeerVar allVar = *((combinePeerVar *)arg);

	struct sockaddr_in client;
	int clientSock;
	int sockaddrLen = sizeof(struct sockaddr_in);

    if(listen(*(allVar.peerAsServerSock), MAX_CLIENTS)==-1){
        perror("listening error");
        exit(-1);
    }

    if((clientSock=accept(*(allVar.peerAsServerSock),(struct sockaddr*)&client,&sockaddrLen))==-1){
           perror("accepting error");
           exit(-1);
    }

    while(1)
    {
        pthread_mutex_lock(&lock);
        char *receivedMessage = receiveMessage(clientSock, buffer);
        pthread_mutex_unlock(&lock);
        
        if(receivedMessage==NULL){
            threadedListen(arg);
        }
        else{
        	// serveChunk:
		    P2PFileDetails a = decodeMessageP2P(receivedMessage);

		    if(a.handler==1){
		    	pthread_mutex_lock(&lock);
		    	serveChunk(clientSock, a.fileName, a.chunkIndicator);
		    	pthread_mutex_unlock(&lock);
		    }
		    free(receivedMessage);
		    close(clientSock);
		}
	}

}

// encode the message to character buffer before passing to the peer
char * encodeMessageP2P(P2PMessage m){
	char *encodedStr;
	int sizeRequired = snprintf(NULL, 0, "%d,%s", m.action, m.arguments)+1;
	encodedStr = malloc(sizeRequired);
	sprintf(encodedStr, "%d,%s", m.action, m.arguments);
	return encodedStr;
}

// decode the message from the character buffer before passing to the peer
P2PFileDetails decodeMessageP2P(char *message){
	char delim[] = ",";
	P2PFileDetails a;
	a.handler = atoi(strtok(message, delim));
	char *token;
	// both cases are the same for now.
	switch(a.handler){
		// serveChunk
		case 1:{
			token = strtok(NULL, delim);
			a.fileName = malloc(strlen(token)+1);
			strcpy(a.fileName, token);
			a.chunkIndicator = atoi(strtok(NULL, delim));
			break;
		}
		// downloadChunk
		case 2:{
			token = strtok(NULL, delim);
			a.fileName = malloc(strlen(token)+1);
			strcpy(a.fileName, token);
			a.chunkIndicator = atoi(strtok(NULL, delim));
			break;
		}
	}
	return a;
}


// return Value 1 means download chunk from other peer; 0 means chunk is already present in the directory
int downloadCheck(char *fileName, int chunkIndicator){
	int chunkExists = 0;
	char *dirPath = malloc(strlen(chunkFold)+strlen(fileName)+1);
	sprintf(dirPath, "%s%s", chunkFold, fileName);

	char *fullFilePath = malloc(strlen(dirPath)+1+9+1); //1 for '/'; 9 for fileName; 1 for '\0';
	sprintf(fullFilePath, "%s/%09d", dirPath, chunkIndicator);

	DIR* dir = opendir(dirPath);

	if(dir){
		if(access(fullFilePath, F_OK)!=-1){
			chunkExists = 1;
		}			 	
	    closedir(dir);
	}

	free(fullFilePath);
	free(dirPath);
	return chunkExists;
}

// to delete the partially downloaded chunk
void deleteChunk(char *fileName, int chunkIndicator){
	char *fullFilePath = NULL;
	int sizeRequired = snprintf(NULL, 0, "%s%s/%09d", chunkFold, fileName, chunkIndicator)+1;
	fullFilePath = malloc(sizeRequired);
	sprintf(fullFilePath, "%s%s/%09d", chunkFold, fileName, chunkIndicator);

	int delStatus = 0;
	if(access(fullFilePath, F_OK)!=-1){
		// 0 means deletion successful
		delStatus = remove(fullFilePath);
	}
	free(fullFilePath);
	return;
}

// update the peer status to ZERO-INACTIVE if the peer's socket does not respond/is close abruptly
void updatePeerStatus(int peerAsServerListeningPort, char *ip, int portNum, int serverSock, struct sockaddr_in serverAddress, int status){
	char buffer[BUFFER_SIZE_NETWORK];
	Message m2;
    m2.action = 5;
	m2.arguments = NULL;
	m2.listeningPort = peerAsServerListeningPort;

	int updatePeerMessageSize = snprintf(NULL, 0, "%s,%d,%d", ip, portNum, status)+1;
	m2.arguments = malloc(updatePeerMessageSize);
	sprintf(m2.arguments, "%s,%d,%d", ip, portNum, 0);

	char *encodedPeerMessage = encodeMessageServer(m2);
	
	serverSock = socket(AF_INET, SOCK_STREAM, 0);
	int connection_status = connect(serverSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	
	if(connection_status!=0){
		printf("SERVER CONNECTION ERROR\n");
		exit(-1);
	}
	sendMessage(serverSock, encodedPeerMessage);
	receiveMessage(serverSock, buffer);

	free(m2.arguments);
	free(encodedPeerMessage);
	close(serverSock);
	return;
}



int main(int argc, char** argv) {
	// Argv[1] - Server Address
	// Argv[2] - PortNumber for the Client
	// Argv[3] - Data Folder Path
	// Argv[4] - Complete Path to the List of Files that the Peer wants to Publish or the complete Path to the folder containing the Files 

	
	// Accept the Inputs and call the init Function
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    int peerAsServerSock = socket(AF_INET, SOCK_STREAM, 0);
    int clientSock;
    int peerAsServerListeningPort = atoi(argv[2]);
    int sockaddr_len = sizeof(struct sockaddr_in);

    char buffer[BUFFER_SIZE_NETWORK];

    struct sockaddr_in serverAddress;
    struct sockaddr_in peerAsServerAddress;
    struct sockaddr_in client;

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[1]));
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    memset(serverAddress.sin_zero, '\0', sizeof(serverAddress.sin_zero));

    peerAsServerAddress.sin_family = AF_INET;
    peerAsServerAddress.sin_port = htons(peerAsServerListeningPort);
    peerAsServerAddress.sin_addr.s_addr = INADDR_ANY;
    memset(peerAsServerAddress.sin_zero, '\0', sizeof(peerAsServerAddress.sin_zero));

    if((serverSock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("server socket error\n");
        exit(-1);
    }
    
    // inititalize peerAsServer socket descriptor
    if((peerAsServerSock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("server socket error\n");
        exit(-1);
    }

    char *dataFolderPath = argv[3];
    int numOfFiles = argc-4;

    int inputSize = 0;
    for(int i=4;i<argc;i++){
    	if(i!=argc-1)
    		inputSize += strlen(argv[i])+1;	// To store delim - ","
    	else
    		inputSize += strlen(argv[i])+1; // To store "\0"
    }


    char publishPath[inputSize];
    int tempLen = 0;
    for(int i=4;i<argc;i++){
    	if(i!=argc-1)
    		tempLen += sprintf(publishPath+tempLen, "%s,", argv[i]);
    	else
    		tempLen += sprintf(publishPath+tempLen, "%s", argv[i]);	

    }

    sprintf(publishPath+tempLen, "%s", "\0");
    char *publishPathCpy = malloc(strlen(publishPath)+1);
    strcpy(publishPathCpy, publishPath);
    // initialize client and get the chunks folder ready
    init(dataFolderPath, publishPathCpy);
    
    
    pthread_t tid; // only one thread to have a non-blocking listening to multiple clients
    int threadVar = 0;

    combinePeerVar allVar;
    allVar.serverSock = &serverSock;
    allVar.peerAsServerSock = &peerAsServerSock;
    allVar.serverAddress = &serverAddress;
    allVar.peerAsServerListeningPort = &peerAsServerListeningPort;

	if(bind(peerAsServerSock, (struct sockaddr*)&peerAsServerAddress, sockaddr_len) == -1){
        perror("binding error\n");
        exit(-1);
    }

    if(pthread_create(&tid, NULL, threadedListen, &allVar)!=0){
    	printf("Failed to create thread\n");
    }

    int connection_status = connect(serverSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if(connection_status!=0){
		printf("SERVER CONNECTION ERROR\n");
		exit(-1);
	}

    registerPeer(serverSock, peerAsServerListeningPort, publishPath, numOfFiles);

    while(1){
    	serverSock = socket(AF_INET, SOCK_STREAM, 0);
    	connection_status = connect(serverSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));

    if(connection_status!=0){
		printf("SERVER CONNECTION ERROR\n");
		exit(-1);
	}
    
    printf("Displaying all the files present in the server\n");

    ArgumentsAll a = getFileList(serverSock, peerAsServerListeningPort);
    for(int i=0;i<a.numOfFiles;i++){
    	printf("FileNumber: %d \t FileName: %s \t\t FileLength: %d\n", i+1, a.fileDetails[i].fileName, a.fileDetails[i].fileLength);
    	// free(a.fileDetails[i].fileName);
    }
    // free(a.fileDetails);

    int fileIndex=0;
    printf("Choose a File Number that you wish to download: ");
    scanf("%d", &fileIndex);
    printf("\n");
    char *fileName = a.fileDetails[fileIndex-1].fileName;
    int totalChunks = ceil(1.0*a.fileDetails[fileIndex-1].fileLength/CHUNK_SIZE);

    char *chunkFilePath;
    int chunkFilePathSize = snprintf(NULL, 0, "%s%s/", chunkFold, fileName)+1;
    chunkFilePath = malloc(chunkFilePathSize);
    sprintf(chunkFilePath, "%s%s/", chunkFold, fileName);

    int chunksDownloaded = getFileCountInDir(chunkFilePath); 

    bool *ongoingDownload = NULL;
    int ongoingDownloadSize = 0;
    bool printFlag = false;

    // keep downloading the chunks until the chunks can be merged to a file
    while(mergeCheck(chunkFold, fileName, p2pFold, totalChunks)!=1){
    	printFlag = true;
    	printf("Chunks Downloaded: %d \t Total Chunks: %d\n", chunksDownloaded, totalChunks);
    	printf("Download Progress: %.2f Percent Complete\n", ceil(100.0*chunksDownloaded/totalChunks));
    	
    	serverSock = socket(AF_INET, SOCK_STREAM, 0);
    	connection_status = connect(serverSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    	if(connection_status!=0){
			printf("SERVER CONNECTION ERROR\n");
			exit(-1);
		}
    	ArgumentsAll b = getFileChunk(serverSock, peerAsServerListeningPort, fileName); 
    	
    	if(b.numOfFiles==0){
    		printf("No Peers available to download this file for now!\n");
    		sleep(1);
    		break;
    	}
    	
    	// initializing ongoingDownload with the number of chunks for a fileName;
    	if(ongoingDownloadSize==0){
    		ongoingDownload = (bool*)malloc(b.numOfFiles*sizeof(bool));
    		memset(ongoingDownload, 0, b.numOfFiles);
    		ongoingDownloadSize = b.numOfFiles;
    	}

    	FileLocationDetails f = chooseRarestChunk(b, ongoingDownload); // free later!
    	ongoingDownload[f.chunkIndicator-1] = true;

	    clientSock = socket(AF_INET, SOCK_STREAM, 0);
	    client.sin_family = AF_INET;
    	client.sin_port = htons(f.portNum);
    	client.sin_addr.s_addr = inet_addr(f.ip); 
    	memset(client.sin_zero, '\0', sizeof(client.sin_zero));
	    connection_status = connect(clientSock, (struct sockaddr *) &client, sizeof(client));

	    // if the client socket is closed (after downloading the complete chunk of the file)
	    if(connection_status!=0){
	    	updatePeerStatus(peerAsServerListeningPort, f.ip, f.portNum, serverSock, serverAddress, 0);
	    	continue;
	    }
    	P2PMessage m1;
    	m1.action = 1;
    	int messageSize = snprintf(NULL, 0, "%s,%d", f.fileName, f.chunkIndicator)+1;
    	m1.arguments = malloc(messageSize);
    	sprintf(m1.arguments, "%s,%d", f.fileName, f.chunkIndicator);
    	char *encodedMessage = encodeMessageP2P(m1);

    	sendMessage(clientSock, encodedMessage);
    	// download chunk instead of a message receive
    	int res = downloadChunk(clientSock, f.fileName, f.chunkIndicator);
    	// if the client socket is closed abruptly (while downloading the file)
    	if(res<0){
    		// update the peer status as offline
			updatePeerStatus(peerAsServerListeningPort, f.ip, f.portNum, serverSock, serverAddress, 0);
    	}
    	else{
    		// printf("Downloaded Chunk %09d for file %s\n", f.chunkIndicator, f.fileName);
    		serverSock = socket(AF_INET, SOCK_STREAM, 0);
	    	connection_status = connect(serverSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	    	if(connection_status!=0){
				printf("SERVER CONNECTION ERROR\n");
				exit(-1);
			}
	    	registerChunk(serverSock, peerAsServerListeningPort, f.fileName, f.chunkIndicator);	
	    	chunksDownloaded = getFileCountInDir(chunkFilePath);
    		ongoingDownload[f.chunkIndicator-1] = false;
    	}


    }
    chunksDownloaded = getFileCountInDir(chunkFilePath);
    // if(printFlag==true){
    	printf("Download Progress: %.2f Percent Complete\n", ceil(100.0*chunksDownloaded/totalChunks));    	
    // }
    // free(ongoingDownload);

    }
    
    // free(publishPathCpy);
	// free(chunkFold);
	// free(p2pFold);
    return 0;

}

