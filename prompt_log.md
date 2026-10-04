# AI Prompt Log

## Entry 1

Date: 03 October 2026

AI Tool:
ChatGPT

Prompt / Purpose:
Asked for a complete explanation of the IE3010 NetMessenger assignment, the required deliverables, marking scheme, report structure, and a step-by-step development plan.

How the Output Was Used:
The explanation was used to understand the assignment requirements and plan the implementation order.

Changes / Verification:
The personalised values for registration number IT23718630 were independently checked against the formulas in the assignment brief before being recorded in the project documentation.
## AI Interaction - Incremental Implementation and Testing

### Purpose
Used ChatGPT to obtain step-by-step guidance while implementing and testing the NetMessenger assignment.

### Assistance Received
- Explained the assignment requirements and personalised values.
- Guided the implementation of the TCP server and client.
- Assisted with POSIX thread-based multi-client handling.
- Helped implement REGISTER, LIST, BCAST, PMSG, JOIN, LEAVE, ROOMS, and RMSG.
- Assisted with SENDFILE framing and exact raw-byte handling.
- Suggested server-side storage structure and file integrity testing.
- Guided graceful and unexpected disconnect testing.
- Helped implement timestamped server logging.
- Suggested and explained the optional rate-limiting and flood-protection mechanism.
- Created a TCP framing test plan for partial commands, multiple commands in one send, malformed commands, and split file data.
- Assisted with README and design-diary preparation.

### Evaluation and Changes
I did not rely only on the generated suggestions. I compiled and ran the program after each major change and tested the functionality using multiple terminal sessions. Where commands or tests produced unexpected results, I checked the server/client behaviour and repeated the tests before continuing.

I verified important protocol responses, NID values, server port, file storage, file integrity, disconnect cleanup, rate limiting, and TCP framing using my own running program.

### What I Learned
The AI assistance helped me understand that TCP is a byte stream and does not preserve application message boundaries. I learned why newline-based framing is needed for text commands and why SENDFILE requires the server to count exactly the declared number of bytes. I also improved my understanding of pthread-based concurrency, shared-state protection with mutexes, graceful versus unexpected disconnections, and server-side rate limiting.
