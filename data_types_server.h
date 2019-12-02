#ifndef DATA_TYPES_SERVER_H
#define DATA_TYPES_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct FileDetails{
        char* fileName;
        int fileLength;
    } FileDetails;

    typedef struct FileLocationDetails{
        char* fileName;
        int chunkIndicator;
        char* ip;
        int portNum;
    } FileLocationDetails;
    
    typedef struct ArgumentsAll
    {   
        int listeningPort;
        int handler;
        int numOfFiles;
        FileDetails *fileDetails;
        FileLocationDetails *fileLocationDetails;
        int peerStatus;
    } ArgumentsAll;

#ifdef __cplusplus
}
#endif

#endif

