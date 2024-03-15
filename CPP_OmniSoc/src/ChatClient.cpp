#include <iostream>
#include <chrono>
#include <thread>
#include "Socket_Serial.h"

int period_ms = 100;
bool killCommand = false;
std::string killInput = "x";

std::atomic<bool> inputAvailable(false);
std::string threadedInput;

void inputThread()
{
	while (!killCommand)
	{
		std::string input;
		std::getline(std::cin, input);
		threadedInput = input;
		if (input == killInput)
		{ killCommand = true; }
		inputAvailable = true;
	}
}


int main() {

	//input timeout
	std::chrono::milliseconds timeout(period_ms);
	

	std::string ip;
	std::string port;
	bool isServer = false;
	bool autoReconnect = false;

	char userInput;
	std::cout << "c:client, s:server" << std::endl;
	std::cin >> userInput;

	if (userInput == 'c' || userInput == 'C') {
		isServer = false;
		std::cout << "enter IP" << std::endl;
		std::cin >> ip;
		std::cout << "enter Port" << std::endl;
		std::cin >> port;
	}
	else if (userInput == 's' || userInput == 'S') {
		isServer = true;
		ip = "127.0.0.1";
		std::cout << "enter Port" << std::endl;
		std::cin >> port;
	}

	std::cout << "auto reconnect: y/n" << std::endl;
	std::cin >> userInput;
	if (userInput == 'y') { autoReconnect = true; }
	else { autoReconnect = false; }

	auto soc = std::make_shared < Socket_Serial>(ip, port, isServer,true);
	std::cout << "socket initialized" << std::endl;

	soc->connect(false, autoReconnect, period_ms);
	//std::cout << "socket connected" << std::endl;

	
	std::thread inputThread(inputThread);
	while (!killCommand)
	{
		//soc->synchronousUpdate();

		std::string outMessage = "";

		if (inputAvailable.exchange(false))
		{
			outMessage = threadedInput;
		}

		if (outMessage.size() > 0)
		{
			soc->send(outMessage);
		}
		

		std::vector<std::string> inMsgs = soc->receive();
		for (int i = 0; i < inMsgs.size(); i++)
		{ 
			std::cout << inMsgs[i] << std::endl; 
		}
	}
	soc->disconnect();

	if (inputThread.joinable())
	{
		inputThread.join();
	}
	

	return 0;
}