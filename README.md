# Peer-to-Peer-Network
To design and implement a simple peer-to-peer (P2P) file sharing system

## System Description
The system can include multiple clients/peers and one central server. A peer will join the P2P file sharing system by connecting to the server and providing a list of files it wants to share. The server shall keep a list of all the files shared on the network. The file being distributed is divided into chunks. For each file, the server will keep track of the list of chunks each peer has. As a peer receives a new chunk of the file, it becomes a source (of that chunk) for other peers. When a peer intends to download a file, it will initiate a direct connection to the relevant peers to download the file.

## Features of the system
### 1. Multiple Connections: 
Peers and servers are able to support multiple connections simultaneously.
### 2. Chunk Selection: 
When a peer downloads chunks from another peer, it downloads uses ”rarest first” approach to determine which chunk to download.
### 3. Chunk Download Completion: 
Upon finishing the download of a chunk, a peer must register that chunk with the server to become a source (of that chunk) for other peers.
### 4. Failure Tolerance: 
The program does not crash if a peer or the server unexpectedly fails or leaves the network. Also, when the peer recovers, it is capable to re-join the network and resume uploading and downloading.

## Running the Program:
### Server Program:
1. gcc Server.c -o server –lsqlite3 –lm –lpthread
2. ./server <PortNumber>
(Sample: ./server 9000)

### Peer Program:
1. gcc Client.c -o client –lm –lpthread
2. ./client <ServerPortNumber> <PeerPortNumber> <DataFolder(where the downloaded file will be placed)> <Directory>
(Sample: ./client 9000, 9001, “../dataFolder/”, “../publishDirectory/”)

Note: Use CreateDB.c to create server (a sqlite3 database which is used to track all the files/chunks in the system)
