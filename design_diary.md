# NetMessenger Design Diary

## 03 October 2026

### Initial Planning
I reviewed the IE3010 Network Programming assignment specification and identified the mandatory requirements for the NetMessenger system.

My personalised values were calculated from registration number IT23718630:
- Port: 14630
- NID:7186
- Source files: server_8630.c and client_8630.c
- Makefile: Makefile_8630
- Log file: netmsg_IT23718630.log
- Storage base path: ./storage/IT23718630/

### Development Decision
I decided to develop the system incrementally. I will first establish a basic TCP connection between one client and the server before adding registration, concurrency, messaging, rooms, file transfer, logging, error handling, and other required functionality.

This approach allows networking problems to be isolated and tested before adding more complex protocol features.
