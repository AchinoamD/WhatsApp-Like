
// -------------------------------------------- Includes -------------------------------------------

#include <map>
#include <iostream>
#include <set>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <cstring>
#include <zconf.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <netdb.h>

// -------------------------------------------- Defines --------------------------------------------

#define INVALID_ARG_MSG "Usage: whatsappServer portNum\n"
#define EXIT_SERVER_MSG "EXIT command is typed: server is shutting down"
#define CATCH_NAME "Client name is already in use.\n"

#define CREATE_GRP_ERR "ERROR: failed to create group \""
#define WHO_MSG ": Requests the currently connected client names.\n"
#define SEND_SUCCESS_MSG "Sent successfully.\n"
#define SEND_ERR_MSG "ERROR: failed to send.\n"
#define EXIT_CLIENT_MSG "Unregistered successfully."

#define CONNECTED " connected"
#define CON_FAIL "Failed to connect the server"
#define CON_SUCCEED "Connected Successfully.\n"

#define VALID_ARG_NUM 2
#define MAX_PENDING_SOCKETS 10

#define NOT_EXIST 0
#define IS_CLIENT_NAME 1
#define IS_GROUP_NAME 2

#define MAX_MSG_LEN 256
#define MAX_HOST_NAME_LEN 30

#define NAME "name"
#define CREATE_GROUP "create_group"
#define SEND "send"
#define WHO "who"
#define EXIT "exit"

#define EXIT_SERVER "EXIT"

#define END_LINE "\n"


// ---------------------------------------- Global variables ---------------------------------------

std::map<std::string, std::set<std::string>> groupsToClients; // Map of the groups and their clients.

std::map<std::string, int> clientsToSockets; // Map clients name to their file descriptor socket.

std::vector<int> fds; // A vector contains all the sockets that connect to the server.

int welcome_socket,
	fd_max;

fd_set clients_fds;
fd_set read_fds;


// ------------------------------------ Function's declarations ------------------------------------

void add_new_client (int current_socket, std::string name);

// ------------------------------------------- Functions -------------------------------------------

/**
 * The function recieve a client name and check if it is already exists as a client name or a
 * group name.
 * @param client_name the client name.
 * @return 0 - not exist, 1 - exist as client, 2 - exist as group.
 */
int is_name_exist (std::string name)
{
	if (groupsToClients.find(name) != groupsToClients.end()){
		return IS_GROUP_NAME;
	}
	if(clientsToSockets.find(name) != clientsToSockets.end())
	{
		return IS_CLIENT_NAME;
	}
	return NOT_EXIST;
}

/**
 * This function check if the client is a member in the group members of the given group name.
 * @param client_name
 * @param group_name
 * @return true if client_name is a member in group_name o.w false
 */
bool is_member(std::string client_name, std::string group_name){
	std::map<std::string, std::set<std::string>>::iterator it = groupsToClients.find(group_name);

	if (it != groupsToClients.end()){ // The group is exist
		std::set<std::string> set = it->second;
		return set.find(client_name) != set.end();
	}
	return false;
//	return groupsToClients[group_name].find(client_name) != groupsToClients[client_name].end();
}

/**
 * Return the client name that cooresponde to the sender_sock.
 * @param sender_sock the sender socket.
 * @return the cooresponde client name.
 */
std::string get_sender_name (int sender_sock){
	for (std::map<std::string, int>::iterator it = clientsToSockets.begin();
	     it != clientsToSockets.end(); ++it){
		if (sender_sock == (*it).second){
			return (*it).first;
		}
	}
	return "";
}

/**
 * This function take care to operate the "who" request.
 * @param sender_sock the client file descriptor.
 */
void server_who(int sender_sock){
	std::string sender_name = get_sender_name(sender_sock);
	std::cout << sender_name << WHO_MSG;
	std::string response = "";
	for(std::map<std::string, int>::iterator it = clientsToSockets.begin();
	    it != clientsToSockets.end(); ++it){
		response += (*it).first;
		response += ",";
	}
	response = response.substr(0, response.length() - 1) + END_LINE;
	if (send(clientsToSockets[sender_name], response.c_str(), response.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
	}
}

/**
 * This function take care to operate the "exit" request.
 * @param sender_sock the client file descriptor.
 */
void server_exit(int sender_sock){
	std::string client_to_remove = get_sender_name(sender_sock);
	for(std::map<std::string, std::set<std::string>>::iterator map_iter = groupsToClients.begin();
		map_iter != groupsToClients.end(); ++map_iter)
	{
		for(std::set<std::string>::iterator it = (*map_iter).second.begin(); it != (*map_iter).second.end(); ++it)
		{
			if((*it).compare(client_to_remove) == 0){
				(*map_iter).second.erase(*it);
				break;
			}
		}
	}
	clientsToSockets.erase(client_to_remove);
	std::vector<int>::iterator elem = std::find(fds.begin(), fds.end(), sender_sock);
	fds.erase(elem);
	FD_CLR(sender_sock, &clients_fds);

	std::string response = EXIT_CLIENT_MSG;
	response += "\n";
	if (send(sender_sock, response.c_str(), response.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
	}
	std::cout << client_to_remove << ": " << EXIT_CLIENT_MSG << std::endl;
}

/**
 * This function take care to operate the "send" request.
 * @param sender_sock the client file descriptor.
 * @param command the details of the message - who receive the message and what is the message.
 */
void server_send(int sender_sock, std::string command){
	std::string sender = get_sender_name(sender_sock);
	std::string receiver = command.substr(0, command.find(" "));
	std::string msg = command.substr(command.find(" ") + 1);
	std::string client_msg = "";

	switch (is_name_exist(receiver)){
		case(IS_CLIENT_NAME):
		{
			std::string receiver_msg = sender + ": " + msg + END_LINE;
			// Send the message to the receiver
			if (send(clientsToSockets[receiver], receiver_msg.c_str(), receiver_msg.length(), 0) < 0) {
				std::cout << "ERROR: send " << errno << "." << std::endl;
				return;
			}
			std::cout << sender + ": \"" + command.substr(command.find(" ") + 1)
			             + "\" was sent successfully to " + receiver + "." << std::endl;

			client_msg = SEND_SUCCESS_MSG;
			// Success message to the sender.
			if (send(sender_sock, client_msg.c_str(), client_msg.length(), 0) < 0) {
				std::cout << "ERROR: send " << errno << "." << std::endl;
				return;
			}
			break;
		}
		case(IS_GROUP_NAME): // receiver = group name
		{
			if(!is_member(sender, receiver)){
				std::cout << sender + ": ERROR: failed to send \"" + msg + "\" to " + receiver +
						"." << std::endl;
				std::string error_msg = SEND_ERR_MSG;
				if (send(sender_sock, error_msg.c_str(), error_msg.length(), 0) < 0) {
					std::cout << "ERROR: send " << errno << "." << std::endl;
					return;
				}
				break;
			}
			std::string receiver_msg = sender + ": " + msg + END_LINE;
			// Send message to all group members.
			for(std::set<std::string>::iterator it = groupsToClients[receiver].begin();
			    it != groupsToClients[receiver].end(); ++it)
			{
				// If the current client is the sender don't send him the message.
				if ((*it).compare(sender) != 0){
					if (send(clientsToSockets[*it], receiver_msg.c_str(), receiver_msg.length(), 0) < 0) {
						std::cout << "ERROR: send " << errno << "." << std::endl;
						return;
					}
				}
			}

			client_msg = SEND_SUCCESS_MSG;
			// Success message to the sender.
			if (send(sender_sock, client_msg.c_str(), client_msg.length(), 0) < 0) {
				std::cout << "ERROR: send " << errno << "." << std::endl;
				return;
			}

			std::cout << sender + ": \"" + command.substr(command.find(" ") + 1)
			             + "\" was sent successfully to " + receiver + "." << std::endl;
			break;
		}
		default: // not exist
		{
			std::cout << sender + ": ERROR: failed to send \"" + msg + "\" to " + receiver + "."
			          << std::endl;
			std::string error_msg = SEND_ERR_MSG;
			if (send(sender_sock, error_msg.c_str(), error_msg.length(), 0) < 0) {
				std::cout << "ERROR: send " << errno << "." << std::endl;
				return;
			}
			break;
		}
	}
}

/**
 * This function take care to operate the "create_group" request.
 * @param sender_sock the client file descriptor.
 * @param command the name of the group to create and it's members.
 */
void server_create_group (int sender_sock, std::string command)
{
	std::string sender = get_sender_name(sender_sock);
	std::string groupName = command.substr(0, command.find(" "));
	std::string members = command.substr(command.find(" ") + 1);

	// If the group name is already exists (as a group name / client name).
	// OR if a client wants to open a group for himself.
	if (is_name_exist(groupName) != 0 || members.compare(sender) == 0){
		std::string err_msg = CREATE_GRP_ERR + groupName + "\".";
		std::cout << sender << ": " << err_msg << std::endl;
		if (send(clientsToSockets[sender], err_msg.c_str(), err_msg.length(), 0) < 0) {
			std::cout << "ERROR: send " << errno << "." << std::endl;
		}
		return;
	}

	// If no error - create group.
	std::set<std::string> group_members;
	std::stringstream stringStream(members);
	std::string token;
	while(getline(stringStream, token, ','))
	{
		// If the current member is a client of the server - we can add it to the group.
		if (clientsToSockets.find(token) != clientsToSockets.end()){
			group_members.insert(token);
		}
		// The current member is not a client of the server - error
		else{
			std::string err_msg = CREATE_GRP_ERR + groupName + "\".";
			std::cout << sender << ": " << err_msg << std::endl;
			err_msg += "\n";
			if (send(clientsToSockets[sender], err_msg.c_str(), err_msg.length(), 0) < 0) {
				std::cout << "ERROR: send " << errno << "." << std::endl;
			}
			return;
		}
	}
	group_members.insert(sender); // The sender is also a member in the group.

	groupsToClients[groupName] = group_members;
	std::string msg = "Group \"" + groupName + "\" was created successfully.";
	std::cout << sender << ": " << msg << std::endl;
	msg += "\n";
	if (send(clientsToSockets[sender], msg.c_str(), msg.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
	}
}

/**
 * This function receive a client request, parse it and send it to the coorespond function to
 * handle it.
 * @param req the request.
 * @param curr_sock the socket file descriptor of the client.
 */
void handle_client_request (char* req, int curr_sock)
{
	std::string request = req;
	request = request.substr(0, request.find(END_LINE));
	std::string operation = request.substr(0, request.find(" "));
	if (operation.compare(NAME) == 0){
		std::string arguments = request.substr(request.find(" ") + 1);
		add_new_client(curr_sock, arguments);
	}
	else if (operation.compare(CREATE_GROUP) == 0){
		std::string arguments = request.substr(request.find(" ") + 1);
		server_create_group(curr_sock, arguments);
	}
	else if (operation.compare(SEND) == 0){
		std::string arguments = request.substr(request.find(" ") + 1);
		server_send(curr_sock, arguments);
	}
	else if (operation.compare(WHO) == 0){
		server_who(curr_sock);
	}
	else if (operation.compare(EXIT) == 0){
		server_exit(curr_sock);
	}
}

/**
 * This function clear all the data structures we used during our program.
 */
void clear_all_data_struct(){

	std::string exit_msg = "exit\n";
	for (unsigned int i = 0; i < fds.size(); i ++){
		if (send(fds[i], exit_msg.c_str(), exit_msg.length(), 0) < 0) {
			std::cout << "ERROR: send " << errno << "." << std::endl;
		}
	}

	fds.clear();

	groupsToClients.clear();
	clientsToSockets.clear();

	FD_ZERO(&clients_fds);
	FD_ZERO(&read_fds);

	close(welcome_socket);
}

/**
 * Create the server socket.
 * @param port_num the saerver port number
 * @return the server socket file descriptor.
 */
int establish_server_socket(char* port_num)
{
	char host_name [MAX_HOST_NAME_LEN];
	int server_socket;
	struct sockaddr_in server_address;
	struct hostent* host;

	if (gethostname(host_name, MAX_HOST_NAME_LEN) == -1){ // Get the host name into host_name.
		std::cout << "ERROR: gethostname " << errno << "." << std::endl;
	}
	if ((host = gethostbyname(host_name)) == NULL){
		std::cout << "ERROR: gethostbyname " << errno << "." << std::endl;
	}

    // sockaddrr_in initialization
	memset(&server_address, 0, sizeof(struct sockaddr_in));
	server_address.sin_family = host->h_addrtype;

	// this is our host address
	memcpy(&server_address.sin_addr, host->h_addr, host->h_length);

	// this is our port number
	server_address.sin_port = htons(atoi(port_num));

	// create socket
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "ERROR: socket " << errno << "." << std::endl;
	}
	if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(struct sockaddr_in)) < 0) {
		std::cout << "ERROR: bind " << errno << "." << std::endl;
		close(server_socket);
	}
	if (listen(server_socket, MAX_PENDING_SOCKETS) == -1) { // max # of queued connects
		std::cout << "ERROR: listen " << errno << "." << std::endl;
		close(server_socket);
	}
	return server_socket;
}

/**
 * This function add new client to the inner server's data structures.
 * @param current_socket - the new socket
 * @param client_name - the client name.
 */
void add_new_client (int current_socket, std::string client_name)
{
	std::string newClient = client_name.substr(0, client_name.find(END_LINE));

	// The client is not already exist
	if (clientsToSockets.find(newClient) == clientsToSockets.end())
	{
		clientsToSockets[newClient] = current_socket;
		std::cout << newClient << CONNECTED << std::endl;
		std::string success_msg = CON_SUCCEED;
		if (send(current_socket, success_msg.c_str(), success_msg.length(), 0) < 0) {
			std::cout << "ERROR: send " << errno << "." << std::endl;
		}
	}
	else // Client name is already exist
	{
		// Remove the socket file descriptor from all lists it's member in.
		FD_CLR(current_socket, &clients_fds);
		std::vector<int>::iterator it = std::find(fds.begin(), fds.end(), current_socket);
		fds.erase(it);

		std::string catch_name = CATCH_NAME;
		if (send(current_socket, catch_name.c_str(), catch_name.length(), 0) < 0) {
			std::cout << "ERROR: send " << errno << "." << std::endl;
		}
		std::cout << CON_FAIL << std::endl;
	}
}


/**
 * Read the client message.
 * @param msg the buffer to accept the client message.
 */
void recv_client_msg (char* msg, int client_socket)
{
	unsigned int b_count = 0; // counts bytes read
	ssize_t br = 0  ; // bytes read this pass

	char * msg_p = msg;
	char end = '\0';
	while (end != '\n') {
		br = (int) recv (client_socket, msg_p, MAX_MSG_LEN - b_count, 0);
		if (br > 0) {
			b_count += br;
			msg_p += br;
		}
		if (br < 1) {
			std::cout << "ERROR: recv " << errno << "." << std::endl;
			if (br == 0){ // The socket was closed.
				// Remove the socket file descriptor from all lists it's member in.
				FD_CLR(client_socket, &clients_fds);
				std::vector<int>::iterator it = std::find(fds.begin(), fds.end(), client_socket);
				fds.erase(it);
			}
			return;
		}
		end = msg[b_count - 1];
	}
}

/**
 * This function accept a new client connection and handle client requests.
 */
void accept_clients_connections ()
{
	struct sockaddr_in client;
	memset(&client, 0, sizeof(struct sockaddr_in));
	int c = sizeof(struct sockaddr_in);

	FD_ZERO(&clients_fds);
	FD_ZERO(&read_fds);
	FD_SET(welcome_socket, &clients_fds);
	FD_SET(STDIN_FILENO, &clients_fds);

	int new_socket;
	int ret_val;
	fd_max = welcome_socket + 1;
	while (true)
	{
		read_fds = clients_fds;

		ret_val = select(fd_max, &read_fds, NULL, NULL, NULL);

		if (ret_val < 0) // System call error
		{
			std::cout << "ERROR: select " << errno << "." << std::endl;
		}

		if (FD_ISSET(welcome_socket, &read_fds)) { // New client is trying to connect the server.
			// Connect new client.
			new_socket = accept(welcome_socket, (struct sockaddr *)&client, (socklen_t*)&c);

			if (new_socket < 0){
				std::cout << "ERROR: accept " << errno << "." << std::endl;
			}

			if (new_socket > fd_max - 1){ // Update the max file descriptor.
				fd_max = new_socket + 1;
			}
			fds.push_back(new_socket);
			FD_SET(new_socket, &clients_fds); // because we need to receive the name from the client.
		}
		else if (FD_ISSET(STDIN_FILENO, &read_fds)) { // Input from the server stdin.

			std::string msg;
			getline(std::cin, msg);
			if (msg.compare(EXIT_SERVER) == 0){
				clear_all_data_struct();
				std::cout << EXIT_SERVER_MSG;
				close(welcome_socket);
				exit(0);
			}
		}
		else { // A new request arrive from one of the clients - to handle.

			for(unsigned int i = 0; i < fds.size(); i ++){
				if (FD_ISSET(fds[i], &read_fds)){
					new_socket = fds[i]; // This is the socket we need to handle.

					char msg [MAX_MSG_LEN];
					// Initialize msg.
					memset(msg, 0, sizeof(msg));

					recv_client_msg(msg, new_socket);
 					handle_client_request(msg, new_socket);
				}
			}
		}
	}
}

/**
 * The main function - responsible to run the whole flow of the server side.
 * @param argc the number of arguments.
 * @param argv the arguments of the program (port number).
 * @return
 */
int main(int argc, char *argv[])
{
	// Validity check.
	if (argc != VALID_ARG_NUM) {
		std::cout << INVALID_ARG_MSG;
		exit(1);
	}

	// Creating the server socket.
	welcome_socket = establish_server_socket(argv[1]);

	// The server accept connections.
	accept_clients_connections();

	// Close the server socket.
	close(welcome_socket);
	return 0;
}