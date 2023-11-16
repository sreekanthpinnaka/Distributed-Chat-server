# Import the necessary modules
import socket
import threading
import queue
import time
# Set up the constants for the server's host, port, and the buffer size
SERVER_HOST = 'localhost'
SERVER_PORT = 5556
BUFFER_SIZE = 1024

# Set up a dictionary to store chat room information
chat_rooms = {}
ook="OK"
def release_variable(room):
    with chat_rooms[room]['mutex']:
        chat_rooms[room]['current_holder'] = None
        # Check if there are any clients waiting and grant access to the next in line
        if not chat_rooms[room]['client_queue'].empty():
            next_client = chat_rooms[room]['client_queue'].get()
            chat_rooms[room]['current_holder'] = next_client
            chat_rooms[room]['hold_until'] = time.time() + 10
            next_client.send(f"{ook}".encode())
            # Start another timer for this client
            release_timer = threading.Timer(10, release_variable, args=[room])
            release_timer.start()
# Define a function to broadcast messages to all clients in the same room
def broadcast_message(room):
    while True:
        # Get the next message from the queue
        timestamp, message, sender_client = chat_rooms[room]['queue'].get()

        # Send the message to all clients in the room, except the sender
        for client in chat_rooms[room]['clients']:
            if client != sender_client:
                client_name = get_client_name(client)
                try:
                    # Try to send the message
                    client.send(f"{client_name}|{timestamp}|{message}".encode())
                except:
                    # If an error occurs, remove the client from the room
                    client.close()
                    remove_client(client, room)

# Define a function to handle client connections
def handle_client(client, room):
    while True:
        try:
            # Try to receive data from the client
            data = client.recv(BUFFER_SIZE)

            # If data was received, process it
            if data:
                # Parse the timestamp and message from the received data
                timestamp, message = data.decode().split('|', 1)
                # Convert the timestamp to an integer
                timestamp = int(timestamp)

                # If the client sent the "/exit" command, remove them from the room and stop the loop
                if message == "/exit":
                    print(f"Client {get_client_name(client)} has disconnected from room {room}")
                    remove_client(client, room)
                    break
                if message.startswith("/request_variable"):
                    with chat_rooms[room]['mutex']:
                        if chat_rooms[room]['current_holder'] is None:
                            chat_rooms[room]['current_holder'] = client
                            client.send(f"{ook}".encode())
                            print(f"{get_client_name(client)} sent a request message to the server.")
                            print("The server responded with an OK message.")
                            # Start a timer to release the variable after 10 seconds
                            release_timer = threading.Timer(10, release_variable, args=[room])
                            release_timer.start()
                            continue
                        else:
                            chat_rooms[room]['client_queue'].put(client)
                            print(f"{get_client_name(client)} sent a request message to the server.")
                            print("The server enqueued the request.")
                            continue
                # Update the room's logical clock
                chat_rooms[room]['logical_clock'] = max(chat_rooms[room]['logical_clock'], timestamp) + 1
                # Add the message to the room's queue
                chat_rooms[room]['queue'].put((chat_rooms[room]['logical_clock'], message, client))
            else:
                # If no data was received, remove the client from the room and stop the loop
                remove_client(client, room)
                break
        except:
            # If an error occurred, remove the client from the room and stop the loop
            remove_client(client, room)
            break

# Define a function to remove a client from a room
def remove_client(client, room):
    # Only try to remove the client if they are in the room
    if client in chat_rooms[room]['clients']:
        chat_rooms[room]['clients'].remove(client)

# Define a function to get a client's name
def get_client_name(client):
    # The client's name is their socket's peer name
    return str(client.getpeername())

# Define a function to start the chat server
def start_chat_server():
    # Create a socket object
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Bind the socket to the host and port
    server_socket.bind((SERVER_HOST, SERVER_PORT))
    # Start listening for incoming connections
    server_socket.listen()
    print(f"Server started on {SERVER_HOST}:{SERVER_PORT}")

    # Main server loop
    while True:
        # Accept an incoming connection
        client_socket, client_address = server_socket.accept()
        print(f"Client connected: {client_address[0]}:{client_address[1]}")
        # Prompt the client to enter a room name
        client_socket.send("Enter chat room name: ".encode())
        # Get the room name from the client
        room = client_socket.recv(BUFFER_SIZE).decode()

        # If the room does not exist, create it
        if room not in chat_rooms:
            chat_rooms[room] = {'clients': [], 
                                'queue': queue.PriorityQueue(),
                                'client_queue': queue.Queue(), 
                                'logical_clock': 0, 
                                'thread_started': False,
                                'client_queue': queue.Queue(),
                                'mutex': threading.Lock(),
                                'current_holder': None,
                                'hold_until': None}

        # Add the client to the room
        chat_rooms[room]['clients'].append(client_socket)

        # If the broadcast thread for this room has not been started, start it
        if not chat_rooms[room]['thread_started']:
            broadcast_thread = threading.Thread(target=broadcast_message, args=(room,))
            broadcast_thread.start()
            chat_rooms[room]['thread_started'] = True

        # Start a new thread to handle the client's connection
        client_thread = threading.Thread(target=handle_client, args=(client_socket, room))
        client_thread.start()

# If this file is being run directly (not being imported), start the server
if __name__ == '__main__':
    start_chat_server()
