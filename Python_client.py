import time
import socket
import threading

SERVER_HOST = 'localhost'
SERVER_PORT = 5556
BUFFER_SIZE = 1024

logical_clock = 0
socket_closed = False  # Flag to indicate if the socket has been closed
socket_lock = threading.Lock()  # Lock to synchronize access to the socket
request_timestamp = None
observed_times = []

def receive_and_display_messages(server_socket):
    global logical_clock
    global socket_closed
    global request_timestamp
    global observed_times
    while True:
        try:
            message = server_socket.recv(BUFFER_SIZE)
            if message:
                decoded_message = message.decode()
                if decoded_message == "OK":
                    elapsed_time = time.time() - request_timestamp
                    observed_times.append(elapsed_time)
            
                    print("Received OK from the server. You now have access to the shared variable for 10 seconds.")
                    # The work is to go to sleep for 10 seconds. It can be changed to anythin, i.e we can modify the variable or do nothing
                    #We can attach the variable in the message and send it back with a diffrent value.
                    time.sleep(10)
                    print("finished with the shared variable")
     
                else:
                    client_name, timestamp, message = decoded_message.split('|', 2)
                    timestamp = int(timestamp)
                    logical_clock = max(logical_clock, timestamp) + 1
                    print(f"({client_name})[{timestamp}] {message}")
        except:
            with socket_lock:
                server_socket.close()
                socket_closed = True
            break


def start_client():
    global logical_clock
    global socket_closed
    global request_timestamp
    global observed_times

    print("Instructions for the client:")
    print("1. Start the client program")
    print("2. When asked for a chat room name, type the name and press Enter")
    print("3. Now you can send messages to the chat room")
    print("4. If you want to exit the chat room, type '/exit' and press Enter")
    print("5. If you want to access the variable, type '/request_variable' and press Enter")


    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((SERVER_HOST, SERVER_PORT))
    print(f"Connected to server {SERVER_HOST}:{SERVER_PORT}")

    room_name = client_socket.recv(BUFFER_SIZE).decode()
    print(room_name)
    room = input()
    client_socket.send(room.encode())

    receive_thread = threading.Thread(target=receive_and_display_messages, args=(client_socket,))
    receive_thread.start()

    while True:
        message = input()
        
        with socket_lock:
            if socket_closed:
                print("Socket is closed. Exiting...")
                break

            if message == "/exit":
                client_socket.send(f"{logical_clock}|{message}".encode())
                print("Disconnected from the server.")
                client_socket.close()
                socket_closed = True
                break
            elif message == "/request_variable":
                client_socket.send(f"{logical_clock}|{message}".encode())
                request_timestamp = time.time()
                logical_clock += 1
            else:
                client_socket.send(f"{logical_clock}|{message}".encode())
                logical_clock += 1
        if len(observed_times) >= 10:
            average_time = sum(observed_times) / len(observed_times)
            print(f"Average Access Time: {average_time:.4f} seconds")

if __name__ == '__main__':
    start_client()
