#include <iostream>
#include <chrono>
#include <thread>
#include "UART_Serial.h"

int period_ms = 1; //arbitrarily low test period.  havent tried sub ms
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

	std::cout << "Omni Soc uart tester." << std::endl;
	std::cout << "x after initial selections to terminate" << std::endl;

	std::string port;
	std::cout << "choose port:" << std::endl;
	std::cin >> port;

	int baudRate = 57600;
	int timeout_ms = 1000;


	auto serial1 = std::make_shared < UART_Serial>(port, baudRate, timeout_ms);
	std::cout << "uart initialized" << std::endl;

	serial1->connect();


	std::thread inputThread(inputThread);
	while (!killCommand)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));//this is optional, but keeps this thread from being a busy thread


		std::string outMessage = "";

		if (inputAvailable.exchange(false))
		{
			outMessage = threadedInput;
		}

		if (outMessage.size() > 0 && serial1->isConnected()) //technically connected check is optional due to buffers, but its bad practice to pack the buffers while socket is disconnected
		{
			//todo: need to convert outMessage string into meaningful uart message (header and floats)
			int arbHeader = 7;
			std::vector<float> arbData = std::vector<float>();
			arbData.push_back(5.123);
			arbData.push_back(1);
			arbData.push_back(2);
			arbData.push_back(5.5);
			serial1->sendMessage(arbHeader, arbData);
		}


		int header = 0;
		std::vector<float> data = std::vector<float>();
		int result = serial1->receiveMessage(header, data);
		if (result == 1)
		{
			std::cout << header << " : ";
			for (int i = 0; i < data.size(); i++)
			{
				std::cout << data[i] << " , ";
			}
			std::cout << std::endl;
		}
		else
		{
			//std::cout << result << std::endl;
		}
	}
	serial1->disconnect();

	if (inputThread.joinable())
	{
		inputThread.join();
	}


	return 0;
}