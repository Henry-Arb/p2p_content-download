# P2P Content Download

This project implements a peer-to-peer (P2P) content sharing network with an index server that helps to manage content discovery among peers. The system allows peers to register, deregister, search, and download content from each other, creating a decentralized network where each peer can act as both a client and a server.

The entire project is implemented in C, utilizing standard libraries for socket programming, file I/O, and data handling.

## Features
- **Content Registration**: Peers can register their content with the index server, additionally providing their peer name and content location
- **Content Deregistration**: Peers can deregister their content when they no longer want to share it.
- **Content Search**: Peers can query the index server for available content and retrieve the port number of the peer that has the content.
- **Content Download/Serving**: Peers can download/serve content directly from/to other peers using TCP connections.

## Technologies and Concepts
- **Socket Programming**: Both UDP and TCP sockets are used in conjunction for different use cases:
- **UDP**: Used for fast communication between peer(s) and the index server.
- **TCP**: Used for peer-to-peer content transfer, providing reliable delivery of data.
- **PDUs**: Custom PDUs with success/error codes are defined for different types of communication.
- **Addressing and Port Management**: Handling IP addresses and port numbers for UDP and TCP communication.
- **Linked Lists**: Used in the index server to maintain a list of registered content and their associated peers and used by peers as a way to store all open sockets.

## Steps to Run:
0. Pre-req: Have a Unix-based system with GCC compiler.
1. Create the executables for index_server.c and peer.c (inside each of the /peer[1, 2, ...] directories):
- `gcc -o index_server index_server.c` 
- `gcc -o peer peer.c`
2. Open at least [N>=2] terminals (one for the index server, the rest for (N-1) peers)
3. Run index server on appropriate port (recommended 16384; default = 3000):
- `./index_server 16384`
4. Run peer executable by providing index server's IP address (`localhost` if on same machine) and same port as used before:
- `./peer localhost 16384`
5. Repeat step 4 for as many peers as required.
