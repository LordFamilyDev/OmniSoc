#include <iostream>
#include <chrono>
#include <thread>
#include "Socket_Serial.h"

int period_ms = 1; //arbitrarily low test period.  havent tried sub ms
bool killCommand = false;
std::string killInput = "x";

std::atomic<bool> inputAvailable(false);
std::string threadedInput;

void inputThread_doWork()
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
	
	bool asyncFlag = true;//false (syncronous mode) is mostly for debugging

	std::string ip;
	std::string port;
	bool isServer = false;
	bool autoReconnect = false;

	std::cout << "Omni Soc chat client." << std::endl;
	std::cout << "x after initial selections to terminate" << std::endl;

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

	auto soc = std::make_shared < Socket_Serial>(ip, port, isServer, asyncFlag);
	std::cout << "socket initialized" << std::endl;

	if (asyncFlag) { soc->connect(false, autoReconnect, period_ms); }
	//std::cout << "socket connected" << std::endl;

	
	std::thread inputThread(inputThread_doWork);
	while (!killCommand)
	{
		if (!asyncFlag) 
		{ 
			soc->synchronousUpdate(); 
			std::this_thread::sleep_for(std::chrono::milliseconds(period_ms)); //this is neccesary with syncrhonous to keep from breaking heartbeat with would_block errors
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));//this is optional, but keeps this thread from being a busy thread
		}

		std::string outMessage = "";

		if (inputAvailable.exchange(false))
		{
			outMessage = threadedInput;
		}

		if (outMessage.size() > 0 && soc->isConnected()) //technically connected check is optional due to buffers, but its bad practice to pack the buffers while socket is disconnected
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