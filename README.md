# NetMessenger

## IE3010 - Network Programming Assignment

NetMessenger is a multi-client chat and file-sharing platform implemented in C using TCP/IP sockets.

The system supports multiple simultaneous clients, user registration, broadcast and private messaging, chat rooms, file transfer, server-side logging, disconnect handling, protocol error handling, and an optional rate-limiting mechanism for basic flood protection.

---

## Student Details

- Registration Number: IT23718630
- Module: IE3010 - Network Programming
- Assignment: NetMessenger - A Multi-Client Chat and File-Sharing Platform over TCP/IP

---

## Personalised Values

The following values were calculated from registration number IT23718630.

- Numeric Part: 23718630
- Last Four Digits: 8630
- Server Port: 6000 + 8630 = 14630
- Node ID: NID:7186
- Server Source File: server_8630.c
- Client Source File: client_8630.c
- Makefile: Makefile_8630
- Log File: netmsg_IT23718630.log
- Server Storage Path: ./storage/IT23718630/<sender_username>/<filename>
- Submission Archive: IT23718630.zip

---

## System Overview

The NetMessenger system consists of one TCP server and multiple TCP clients.

The server listens on port 14630 and handles multiple connected clients using POSIX threads.

Each connected client is handled by a separate server thread.

Shared data structures such as the connected client list and chat room information are protected using mutexes.

---

## Main Features

The implementation supports the following features:

- User registration with unique usernames
- Listing currently connected users
- Broadcast messaging
- Private messaging
- Chat room creation and joining
- Leaving chat rooms
- Listing available chat rooms
- Room messaging
- User-to-user file transfer
- Room-based file transfer
- Server-side storage of transferred files
- Graceful client disconnection
- Detection and cleanup of unexpected client disconnections
- Server-side event logging
- Invalid command and malformed input handling
- TCP framing support
- Exact file byte counting
- Per-client rate limiting and basic flood protection
- User join and leave presence notifications

---

## Communication Protocol

### REGISTER

Format:

```text
REGISTER <username>

Presence notifications:

When another user registers:

MSG JOIN <username>

When a registered user disconnects:

MSG LEAVE <username>
