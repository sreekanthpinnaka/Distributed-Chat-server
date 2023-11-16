#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <queue>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <algorithm>

#define BUFFER_SIZE 1024
#define MAX_USERS 10
#define SERVER_PORT 8081

struct Message {
    int timestamp;
    std::string content;
    bool operator<(const Message& other) const {
        return timestamp > other.timestamp;
    }
};
std::map<int, int> userIdToSocket;

std::map<int, std::vector<int>> userRooms;
std::map<int, std::priority_queue<Message>> roomMessages;
std::mutex roomsMutex;
int sharedVariable = 0;  // The shared variable
std::priority_queue<Message> requestQueue;  // Queue to store client access requests

void handleUser(int user_socket, int user_id) {
    char dataBuffer[BUFFER_SIZE];
    std::string message = "User #" + std::to_string(user_id) + " has joined the server\n";
    std::cout << message;

    int currentRoomID = -1;
    int serverLamportClock = 0;
    userIdToSocket[user_id] = user_socket;

    while (true) {
        memset(dataBuffer, 0, sizeof(dataBuffer));
        std::string receivedData;

        if (read(user_socket, dataBuffer, sizeof(dataBuffer)) != 0) {
            receivedData = dataBuffer;
            size_t firstColonPos = receivedData.find(":");
            size_t secondColonPos = receivedData.find(":", firstColonPos + 1);

          

            std::string timestamp = receivedData.substr(0, firstColonPos);
            int userLamportClock = std::stoi(timestamp);
            serverLamportClock = std::max(serverLamportClock, userLamportClock) + 1;
            
            if (receivedData.substr(firstColonPos + 1) == "request_access") {
		    std::cout << "P" << user_id << " sent a request message to the server.\n";
		        // Enqueue the request
		    Message newRequest = {userLamportClock, "request:" + std::to_string(user_id)};
		    requestQueue.push(newRequest);
		    
			    // Check if this client's request is at the top and the shared variable is not accessed by other clients
		    if (requestQueue.top().content == "request:" + std::to_string(user_id) && sharedVariable == 0) {
		        std::cout << "The server responded with an OK message to P" << user_id << ".\n";
		        std::string response = std::to_string(serverLamportClock) + ":OK:" + std::to_string(sharedVariable);
		       
		        send(user_socket, response.c_str(), response.size(), 0);
		        sharedVariable = user_id;  // Mark the shared variable as accessed by this client
		         } else {
		          std::cout << "The server enqueued the request from P" << user_id << ".\n";
            }}
            else if (receivedData.substr(firstColonPos + 1) == "release") {
		    std::cout << "P" << user_id << " sent a release message to the server.\n";
		    
		    // Remove the request from the queue and release the shared variable
		    requestQueue.pop();
		    sharedVariable = 0;

		    // Check the next client's request
		    if (!requestQueue.empty()) {
		        int nextClient = std::stoi(requestQueue.top().content.substr(8)); // Extract the user id from "request:<id>"
		        std::cout << "The server sent an OK message to P" << nextClient << ".\n";
		        std::string response = std::to_string(serverLamportClock) + ":OK:" + std::to_string(sharedVariable);
		        int userSocket = userIdToSocket[nextClient];
		        send(userSocket, response.c_str(), response.size(), 0);
		        sharedVariable = nextClient; // Mark the shared variable as accessed by this client
		    }

        }
         else if (receivedData.substr(firstColonPos + 1, secondColonPos - firstColonPos - 1) == "update") {
            int newValue = std::stoi(receivedData.substr(secondColonPos + 1));
            sharedVariable = newValue;  // Update the shared variable's value
        }
        
            else if (receivedData.substr(firstColonPos + 1, secondColonPos - firstColonPos - 1) == "room_change") {
                // Remove the user from the current room
                auto &currentRoomUsers = userRooms[currentRoomID];
                currentRoomUsers.erase(std::remove(currentRoomUsers.begin(), currentRoomUsers.end(), user_socket), currentRoomUsers.end());

                // Change the user's room
                currentRoomID = std::stoi(receivedData.substr(secondColonPos + 1, receivedData.find(":", secondColonPos + 1) - secondColonPos - 1));

                // Add the user to the new room
                std::lock_guard<std::mutex> lock(roomsMutex);
                userRooms[currentRoomID].push_back(user_socket);
            } else {
                firstColonPos = receivedData.find(":");
            	secondColonPos = receivedData.find(":", firstColonPos + 1);
              	if (firstColonPos == std::string::npos || secondColonPos == std::string::npos) {
               			std::cout << "Invalid message format from user " << user_id << "\n";
                		continue;
           			 }
                if (currentRoomID == -1) {
                    currentRoomID = std::stoi(receivedData.substr(firstColonPos + 1, secondColonPos - firstColonPos - 1));
                    std::lock_guard<std::mutex> lock(roomsMutex);
                    userRooms[currentRoomID].push_back(user_socket);
                }

                std::string messageContent = receivedData.substr(secondColonPos + 1);
                Message newMessage = {serverLamportClock, "user" + std::to_string(user_id) + ": " + messageContent};
               
                roomMessages[currentRoomID].push(newMessage);
                for (int user: userRooms[currentRoomID]) {
                	if(user!=user_socket){
                    std::string timestampedMessage = std::to_string(newMessage.timestamp) + ":" + newMessage.content;

                    send(user, timestampedMessage.c_str(), timestampedMessage.size(), 0);
                }}
            }
        } else {
            std::cout << "User #" << user_id << " has disconnected\n";
            break;
        }
    }
    close(user_socket);
}

int main() {
    int serverSocket;
    struct sockaddr_in serverAddress;
    int opt = 1;
    int addressLength = sizeof(serverAddress);

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running...\n";

    int userId = 0;

    while (true) {
        int new_socket;
        if ((new_socket = accept(serverSocket, (struct sockaddr *)&serverAddress, (socklen_t*)&addressLength)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        std::thread userThread(handleUser, new_socket, userId++);
        userThread.detach();
    }

    return 0;
}

