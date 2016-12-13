# Betting-Game-Server


This game server accepts clients on TCP sockets, it allows the client to pick a number within a published range, and every 15 seconds randomize a winning number, and then lets the clients know if they have won or not.

The program accepts both IPv4 and IPv6 connections on TCP port and handle up to BEGASEP_NUM_CLIENTS clients. The code must allow for this define to range from 1 to 64500.
An unique client id is generated upon receipt of a BEGASEP_OPEN message, this client id never collides with another connected client.

If the maximum number of simultaenous clients connected is reached the server tears down the connection, else it responds to the BEGASEP_OPEN  message with a BEGASEP_ACCEPT message with the allocated client id and the allowed betting number range.

When the client has sent a BEGASEP_BET message, which follow the message BEGASEP_ACCEPT and have a betting number within the published range, the server is to include that client in the next betting run.
The client only send the BEGASEP_BET message once, any protocol  breaches must immediately result in a connection teardown by the server.
A betting run is performed every 15 seconds but only if there are  clients connected.

The betting run generates a random number within the published betting number range and broadcast the winning number to all clients with the BEGASEP_RESULT message, indicating for each client if they won or not. 
Each winner also prints on a seperate line to stdout, syslog, or similar.

The connection to each client that betted in this run is then closed.

The program is able to run "forever" without interaction
