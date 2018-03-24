all: whatsappServer whatsappClient

whatsappServer: whatsappServer.cpp
	g++ -Wall -Wextra -std=c++11 whatsappServer.cpp -o whatsappServer

whatsappClient: whatsappClient.cpp
	g++ -Wall -Wextra -std=c++11 whatsappClient.cpp -o whatsappClient

clean:
	rm whatsappClient whatsappServer

tar:
	tar -cvf ex5.tar whatsappServer.cpp whatsappClient.cpp Makefile README
