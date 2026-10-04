# NetMessenger Design Diary

## Student
K.K. Pavani Sathsarani Kannangara  
Registration Number: IT23718630

## Initial Planning

I first reviewed the assignment specification and calculated all personalised values from my registration number.

- Port: 14630
- NID:7186
- Server file: server_8630.c
- Client file: client_8630.c
- Makefile: Makefile_8630
- Log file: netmsg_IT23718630.log
- Storage path: ./storage/IT23718630/

I decided to build the system incrementally rather than implementing all features at once. I first created a basic TCP server and client connection and verified that the server could bind, listen, accept a connection, and communicate with a client.

## Concurrency Decision

The server needed to support several clients simultaneously. I selected POSIX threads as the concurrency model because each connected client could be handled independently while the main thread continued accepting new connections.

Shared information such as connected users and room membership is protected using mutexes to reduce race conditions.

## Registration and Messaging

After the basic connection worked, I implemented REGISTER and LIST. I added duplicate username checking and enforced the requirement that REGISTER must be the first command.

I then implemented broadcast and private messaging. Broadcast messages are forwarded to all other registered clients, while private messages are sent only to the requested username.

## Chat Rooms

The next stage added JOIN, LEAVE, ROOMS, and RMSG. A room is created automatically when the first user joins it.

I also made sure that a client leaving or unexpectedly disconnecting is removed from its room memberships.

## File Transfer

File transfer was one of the more complex parts because the SENDFILE command is followed by raw bytes.

The server reads exactly the declared file size rather than assuming that one recv() call contains the whole file.

A server-side copy is saved under the personalised storage path:

./storage/IT23718630/<sender_username>/<filename>

I tested file integrity using cmp and confirmed that both the server copy and receiver copy were identical to the original file.

## Error Handling and TCP Framing

I added consistent error responses for invalid commands, unknown users, missing rooms, incorrect room membership, duplicate usernames, and invalid file requests.

I also tested TCP framing cases where a command was split across multiple sends and where several commands arrived together. A Python test script was created for these tests.

The file-transfer logic also preserves framing by consuming raw bytes where necessary when a SENDFILE request is rejected.

## Logging and Disconnect Handling

The server records timestamped events in:

netmsg_IT23718630.log

The log includes connections, registrations, messages, file transfers, invalid commands, and disconnects.

I tested both graceful disconnection using QUIT and an unexpected disconnection by terminating a client process. The server remained active and removed the disconnected user from the active user list.

## Optional Extension

I implemented per-client rate limiting as the optional extension.

A client can send up to 10 normal commands within a 5-second window. Excess commands receive:

ERR 008 RATE_LIMIT_EXCEEDED NID:7186

QUIT is still allowed so a client can disconnect cleanly after reaching the rate limit.

## Final Testing

The final implementation was tested with five simultaneously connected clients.

I also re-tested registration, LIST, broadcast messaging, private messaging, room operations, file transfer, file integrity, malformed commands, disconnect handling, rate limiting, and TCP framing.

The incremental implementation approach helped isolate errors and made it easier to understand and test each networking feature before moving to the next stage.
