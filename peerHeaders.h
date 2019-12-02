#ifndef PEERHEADERS_H
#define PEERHEADERS_H

#ifdef __cplusplus
extern "C" {
#endif
	#include <stdio.h>
	#include <string.h>
	#include <unistd.h>
	#include <stdlib.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <dirent.h>
	#include <errno.h>
	#include <libgen.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <math.h>

    #include <sqlite3.h>
    #include <pthread.h>
    
    #include <netinet/in.h>
    #include<arpa/inet.h>
    #include <netdb.h>
    #include <fcntl.h>
	#include <sys/sendfile.h>
    #include <stdbool.h>

	#define CHUNK_SIZE 512
	#define BUFFER_SIZE_NETWORK 512
	#define CHUNK_PATH "split --numeric-suffixes=1 -a 9 -b %d \"%s\" \"%s/\""
    #define MAX_CLIENTS 10


	int isRegularFile(const char *path)
	{
	    struct stat path_stat;
	    stat(path, &path_stat);
	    return S_ISREG(path_stat.st_mode);
	}


	char* createChunkDirectory(char *fileName, char *chunkFolderPath){
		char *fullPath;
		struct stat st = {0};
		if((fullPath = malloc(strlen(fileName)+strlen(chunkFolderPath)))){
			strcat(fullPath, chunkFolderPath);
			strcat(fullPath, fileName);
		}
		else{
			fprintf(stderr, "Malloc Failed\n");
		}
		if(stat(fullPath, &st)==-1){
			mkdir(fullPath, 0777);
		}

		return fullPath;
	}


	void chunkFile(char *absoluteFilePath, char *chunkFolderPath){
		char *baseFileName = basename(absoluteFilePath);
		char *dirPath = createChunkDirectory(baseFileName, chunkFolderPath);

		char *command;

		size_t sizeRequired = snprintf(NULL, 0, CHUNK_PATH, CHUNK_SIZE, absoluteFilePath, dirPath)+1;
		command = malloc(sizeRequired*sizeof(char));
		sprintf(command, CHUNK_PATH, CHUNK_SIZE, absoluteFilePath, dirPath);
		system(command);
		free(command);
		free(dirPath);
		return;
	}

	void chunkDir(char *fileLocation, char *chunkFolderPath){
		DIR *dir;
		struct dirent *ent;
		char *absolutePath;

		if((dir = opendir(fileLocation)) != NULL){
	  		/* print all the files and directories within directory */
	  		while((ent = readdir (dir)) != NULL){
	  			if (!strcmp(ent->d_name, "."))
	            	continue;
	        	if (!strcmp(ent->d_name, ".."))    
	            	continue;
	            if(!strcmp(ent->d_name, fileLocation))
	            	continue;
	            absolutePath = malloc(strlen(fileLocation)+strlen(ent->d_name)+1);
	            sprintf(absolutePath, "%s%s", fileLocation, ent->d_name);
	            if(isRegularFile(absolutePath))
	    			chunkFile(absolutePath, chunkFolderPath);
	    		free(absolutePath);
	  		}
	  		closedir(dir);
		} 
		else {
	  		/* could not open directory */
	  		perror("Could Not Open the Directory!!");
	  		exit(-1);
		}
		return;
	}

	void removeAllDirectories(char *chunkFolderPath){
		size_t sizeRequired = snprintf(NULL, 0, "rm -rf %s/*/", chunkFolderPath);
		char *command = malloc(sizeRequired*sizeof(char));
		sprintf(command, "rm -rf %s/*/", chunkFolderPath);
		system(command);
		free(command);
		return;
	}

	size_t getChunkCount(char *absoluteFilePath){
		char *baseDirName = basename(absoluteFilePath);
		size_t chunkCount=0;
		struct stat st = {0};
		DIR *dir;
		struct dirent *ent;
		if(stat(absoluteFilePath, &st) == -1) {
	    	mkdir(absoluteFilePath, 0777);
		}
		if((dir = opendir(absoluteFilePath)) != NULL){
	  		/* print all the files and directories within directory */
	  		while((ent = readdir(dir)) != NULL){
	  			if (!strcmp(ent->d_name, "."))
	            	continue;
	        	if (!strcmp(ent->d_name, ".."))    
	            	continue;
	            if(!strcmp(ent->d_name, baseDirName))
	            	continue;
	            
	            ++chunkCount;
	        }
	        closedir(dir);
	    } 
		else {
	  		/* could not open directory */
	  		perror("Could Not Open the Directory!!");
	  		chunkCount = -1;
		}
		return chunkCount;     
	}

	int isMerge(char *absoluteFilePath, size_t totalChunks){
		int boolean = 0;
		size_t chunkCount = getChunkCount(absoluteFilePath);

		if(chunkCount==totalChunks)
			boolean=1;
		else
			boolean=0;
		return boolean;
	}


    typedef struct Message{
    	int listeningPort;
        int action;
        char *arguments;
    } Message;

    typedef struct combinePeerVar{
    	int *serverSock;
    	int *peerAsServerSock;
	    struct sockaddr_in *serverAddress;
    	int *peerAsServerListeningPort;

    }combinePeerVar;

    typedef struct  P2PFileDetails{
    	int handler;
    	char *fileName;
    	int chunkIndicator;
    }P2PFileDetails;

    typedef struct P2PMessage{
    	int action;
    	char *arguments;
    }P2PMessage;
    
    typedef struct ParallelDownloadThreadArgs{
    	char *fileName;
    	bool *ongoingDownload;
    	int *ongoingDownloadSize;
    	int *peerAsServerListeningPort;
    	int *serverListeningPort;
    	bool *noPeerFlag;
    	int *currentChunkCount;
    	int *chunksInFolder;
    }ParallelDownloadThreadArgs;
    
#ifdef __cplusplus
}
#endif

#endif /* PEERHEADERS_H */

