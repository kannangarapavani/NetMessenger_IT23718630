import socket
import time

HOST = "127.0.0.1"
PORT = 14630


def recv_line(sock):
    data = b""

    while not data.endswith(b"\n"):
        chunk = sock.recv(1)

        if not chunk:
            break

        data += chunk

    return data.decode(errors="replace").strip()


def test_partial_register():
    print("\nTEST 1 - Partial REGISTER command")

    s = socket.create_connection((HOST, PORT))

    s.sendall(b"REG")
    time.sleep(0.2)

    s.sendall(b"ISTER framinguser\n")

    response = recv_line(s)

    print("Response:", response)

    if response == "OK REGISTERED framinguser NID:7186":
        print("PASS - Partial command reconstructed correctly")
    else:
        print("FAIL")

    s.sendall(b"QUIT\n")
    print("QUIT:", recv_line(s))

    s.close()


def test_multiple_commands():
    print("\nTEST 2 - Multiple commands in one TCP send")

    s = socket.create_connection((HOST, PORT))

    s.sendall(
        b"REGISTER multiuser\n"
        b"LIST\n"
        b"ROOMS\n"
    )

    r1 = recv_line(s)
    r2 = recv_line(s)
    r3 = recv_line(s)

    print("Response 1:", r1)
    print("Response 2:", r2)
    print("Response 3:", r3)

    if (
        r1 == "OK REGISTERED multiuser NID:7186"
        and r2.startswith("OK USERS ")
        and r3.startswith("OK ROOMS ")
    ):
        print("PASS - Multiple commands handled correctly")
    else:
        print("FAIL")

    s.sendall(b"QUIT\n")
    print("QUIT:", recv_line(s))

    s.close()


def test_malformed_command():
    print("\nTEST 3 - Malformed / invalid command")

    s = socket.create_connection((HOST, PORT))

    s.sendall(b"REGISTER malformeduser\n")

    print("Register:", recv_line(s))

    s.sendall(b"THIS_IS_NOT_VALID\n")

    response = recv_line(s)

    print("Response:", response)

    if response == "ERR 005 INVALID_COMMAND NID:7186":
        print("PASS - Invalid command rejected without server crash")
    else:
        print("FAIL")

    s.sendall(b"LIST\n")

    response = recv_line(s)

    print("Health check:", response)

    if response.startswith("OK USERS "):
        print("PASS - Server still responsive after invalid command")
    else:
        print("FAIL")

    s.sendall(b"QUIT\n")
    print("QUIT:", recv_line(s))

    s.close()


def test_split_file_bytes():
    print("\nTEST 4 - SENDFILE bytes split across sends")

    sender = socket.create_connection((HOST, PORT))
    receiver = socket.create_connection((HOST, PORT))

    sender.sendall(b"REGISTER filesender\n")
    receiver.sendall(b"REGISTER filereceiver\n")

    print("Sender register:", recv_line(sender))
    print("Receiver register:", recv_line(receiver))

    file_data = b"ABCDE12345"

    header = (
        b"SENDFILE filereceiver framing.txt 10\n"
    )

    sender.sendall(header)

    sender.sendall(file_data[:3])
    time.sleep(0.1)

    sender.sendall(file_data[3:7])
    time.sleep(0.1)

    sender.sendall(file_data[7:])

    sender_response = recv_line(sender)

    print("Sender response:", sender_response)

    receiver_header = recv_line(receiver)

    print("Receiver header:", receiver_header)

    received_bytes = b""

    while len(received_bytes) < 10:
        chunk = receiver.recv(10 - len(received_bytes))

        if not chunk:
            break

        received_bytes += chunk

    print("Received bytes:", received_bytes)

    if (
        sender_response
        == "OK FILE_RECEIVED framing.txt NID:7186"
        and receiver_header
        == "MSG FILE filesender framing.txt 10"
        and received_bytes == file_data
    ):
        print("PASS - Exact file bytes reconstructed correctly")
    else:
        print("FAIL")

    sender.sendall(b"QUIT\n")
    print("Sender quit:", recv_line(sender))

    receiver.sendall(b"QUIT\n")
    print("Receiver quit:", recv_line(receiver))

    sender.close()
    receiver.close()


if __name__ == "__main__":
    test_partial_register()
    test_multiple_commands()
    test_malformed_command()
    test_split_file_bytes()

    print("\nALL FRAMING TESTS COMPLETED")
