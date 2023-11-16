#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <algorithm>

#define SERVER_PORT 8081
#define BUFFER_SIZE 1024
#include <vector>
#include <chrono>
std::vector<std::chrono::duration<double>> accessTimes;
void receiveMessages(int clientSocket, int* lamportClock,std::chrono::high_resolution_clock::time_point& start) {
    char dataBuffer[BUFFER_SIZE] = {0};

    while (true) {
        memset(dataBuffer, 0, sizeof(dataBuffer));
        int readSize = read(clientSocket, dataBuffer, BUFFER_SIZE);

        if (readSize > 0) {
            std::string receivedData = dataBuffer;
            size_t firstColonPos = receivedData.find(":");
            int receivedLamportClock = std::stoi(receivedData.substr(0, firstColonPos));
            *lamportClock = std::max(*lamportClock, receivedLamportClock);
            std::cout << "LC: " << receivedLamportClock << " " << receivedData.substr(firstColonPos + 1) << std::endl;
            if (receivedData.find(":OK:") != std::string::npos) {
                    std::cout << "Received OK from the server.\n";
                    auto end = std::chrono::high_resolution_clock::now();
        	    std::chrono::duration<double> elapsed = end - start;
        	    accessTimes.push_back(elapsed);
        	if(accessTimes.size() >= 10) {
           		 double total = 0.0;
           		 for(auto& time : accessTimes) {
          		      total += time.count();
            			}	
          	  std::cout << "Average access time: " << (total / 10.0) << " seconds\n";
            	  accessTimes.clear();
        		}
                    int sharedVarValue = std::stoi(receivedData.substr(receivedData.rfind(":") + 1));
                    
                    // Modify the shared variable (e.g., increment its value)
                    sharedVarValue++;
                 
                    std::string updateMsg = std::to_string(*lamportClock) + ":update:" + std::to_string(sharedVarValue);
                    send(clientSocket, updateMsg.c_str(), updateMsg.size(), 0);

                    // Sleep for 10 seconds
                    std::this_thread::sleep_for(std::chrono::seconds(10));

                    // Send release message to the server
                    std::string releaseMsg = std::to_string(*lamportClock) + ":release";
                    send(clientSocket, releaseMsg.c_str(), releaseMsg.size(), 0);
                    }
        } else {
            break;
        }
    }
}

int main() {
    int clientSocket = 0;
    struct sockaddr_in serverAddress;
    std::chrono::high_resolution_clock::time_point start;

    int lamportClock = 0;
    std::cout<<"To enter the room use the following message format 'roomID: Any message'  \n";
    std::cout<<"The same format is used to send messages in the chat room\n";
    std::cout<<"example '1:enter'. \n";
    std::cout<<"The First message only connects you to the chat room. \n";
    std::cout<<"Any messages sent after that are considered as actual messages\n";
    std::cout<<"To exit the program type './exit'\n";
    std::cout<<"To switch rooms, simply change the roomID in your message.\n";
    std::cout<<"The first message used to change will not be broadcasted. It is only to enter the chat room\n";
    std::cout<<"Now to access the shared variable type 'request_access'. This should send the request to the server.\n";


    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "\n Error in socket creation \n";
        return -1;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if(inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        std::cout << "\nInvalid address/ Address not supported \n";
        return -1;
    }

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cout << "\nConnection Failed \n";
        return -1;
    }

std::thread receiveThread(receiveMessages, clientSocket, &lamportClock, std::ref(start));

    receiveThread.detach();

    std::string userInput, prevUserInputPrefix = "";
   
  while (true) {
    std::getline(std::cin, userInput);

    if (userInput == "./exit") {
        close(clientSocket);
        exit(0);
    }

    if (userInput == "request_access") {
        lamportClock++;
        std::string requestMsg = std::to_string(lamportClock) + ":request_access";
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

        send(clientSocket, requestMsg.c_str(), requestMsg.size(), 0);
       
        continue;
    } 
        lamportClock++;
    std::string timestampedMessage;

    // Extract prefix before the first colon
    std::string currentPrefix = (userInput.find(":") != std::string::npos) ? userInput.substr(0, userInput.find(":")) : "";

    if (currentPrefix != prevUserInputPrefix) {
        timestampedMessage = std::to_string(lamportClock) + ":room_change:" + userInput;
        prevUserInputPrefix = currentPrefix;
    } else {
        timestampedMessage = std::to_string(lamportClock) + ":" + userInput;
    }

    send(clientSocket, timestampedMessage.c_str(), timestampedMessage.size(), 0);
}

    return 0;
}


