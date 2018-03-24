
// -------------------------------------------- Includes -------------------------------------------

#include <iostream>
#include <vector>
#include <regex>
#include <set>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// -------------------------------------------- Defines --------------------------------------------

#define VALID_ARG_NUM 4

#define CREATE_GROUP "create_group "
#define SEND "send "
#define WHO "who"
#define EXIT "exit"

#define END_LINE "\n"
#define WHO_COMMAND "who\n"
#define EXIT_COMMAND "exit\n"

#define INVALID_ARG "Usage: whatsappClient clientName serverAddress serverPort\n"
#define CON_FAIL "Failed to connect the server\n"
#define CATCH_NAME "Client name is already in use.\n"
#define CON_SUCCEED "Connected Successfully\n"
#define INVALID_COMMAND "ERROR: Invalid input."

#define CREATE_GRP_FAILED "ERROR: failed to create group \""
#define SEND_FAILED "ERROR: failed to send."
#define WHO_FAILED "ERROR: failed to receive list of connected clients."

#define MAX_MSG_LEN 257

// ---------------------------------------- Global variables ---------------------------------------

int sockfd;
std::string client_name;

// ------------------------------------------- Functions -------------------------------------------

/**
 * This function check the legality of the given name - that it contains only letters and digits.
 * @param name the name to check.
 * @return true if the name is legal and false otherwise.
 */
bool check_name_legality (std::string name)
{
//	std::regex regex("\\w+");
	std::regex regex("(([A-Z])|([a-z])|([0-9]))+");
	std::smatch match;
	return std::regex_match(name, match, regex);
}

/**
 * A help function for the create_group operation. This function clean the given list of users
 * (that need to be members in the group), it removes all the illegal clients names (accordding
 * to the check_name_legality function).
 * @param list_of_client_names a string that contains the names of the group members.
 * @return a string of all the legal clients names.
 */
std::string parse_list_of_clients (std::string list_of_client_names)
{
	std::set<std::string> set_of_clients;
	std::stringstream stringStream(list_of_client_names);
	std::string token;
	while(getline(stringStream, token, ','))
	{
		if (token.compare("") != 0){
			if (check_name_legality(token))
			{
				set_of_clients.insert(token);
			}
		}
	}

	if (set_of_clients.size() < 1){
		return "";
	}

	std::ostringstream stream;
	std::copy(set_of_clients.begin(), set_of_clients.end(), std::ostream_iterator<std::string>(stream, ","));
	std::string result = stream.str();
	result = result.substr(0, result.length() - 1);
	return result;
}

/**
 * This function read a message that was sent to the client by the server and print it.
 * @param msg - pointer that will contain the receive message.
 */
void client_recv_server_msg (char* msg)
{
	unsigned int b_count = 0; // counts bytes read
	ssize_t br = 0  ; // bytes read this pass

	char * msg_p = msg;
	char end = '\0';
	while (end != '\n') {
		br = (int) recv (sockfd, msg_p, MAX_MSG_LEN - b_count, 0);
		if (br > 0) {
			b_count += br;
			msg_p += br;
		}
		if (br == 0){ // Server terminated.
			std::cout << msg;
			close(sockfd);
			exit(1);
		}
		if (br == -1) {
			std::cout << "ERROR: recv " << errno << "." << std::endl;
			close(sockfd);
			exit(1);
		}
		end = msg[b_count - 1];
	}

	std::string str_msg(msg);

	// When the server shutdown using EXIT command from the user - no need to print the exit message
	if (str_msg.compare(EXIT_COMMAND) == 0){
		close(sockfd);
		exit(1);
	}
	else{
		std::cout << msg;
	}
}

/**
 * create_group operation, in a case the user want to create some group.
 * @param command the user command.
 */
void client_create_group (std::string command)
{
	std::string grp_name = command.substr(0, command.find(" "));
	// The command structure is invalid (include the validity check about the grp_name).
//	std::regex regex("\\w+\\s(\\w+\\,)*\\w+");
	std::regex regex("(([A-Z])|([a-z])|([0-9]))+\\s((([A-Z])|([a-z])|([0-9]))+\\,)*(([A-Z])|([a-z])|([0-9]))+");
	std::smatch match;
	if (!std::regex_match(command, match, regex)){ // Check that the command is valid
		std::cout << CREATE_GRP_FAILED << grp_name << "\"." << std::endl;
		return;
	}

	// list_of_client_names is empty. i.e empty group. (The command structure is invalid)
	if(parse_list_of_clients(command.substr(command.find(" ") + 1)).compare("") == 0){
		std::cout << CREATE_GRP_FAILED << grp_name << "\"." << std::endl;
		return;
	}

	// The group name is equal to the client name.
	if (grp_name.compare(client_name) == 0){
		std::cout << CREATE_GRP_FAILED << grp_name << "\"." << std::endl;
		return;
	}

	// Create the request to the server.
	std::string request = CREATE_GROUP + command + END_LINE;

	// We can send the request to the server.
	if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
		close(sockfd);
		exit(1);
	}

	// Receive the server response.
	char msg [MAX_MSG_LEN];
	memset(msg, 0, sizeof(msg));
	client_recv_server_msg(msg);
}

/**
 * send operation, in a case the user want to send a message to someone.
 * @param command the user command.
 */
void client_send (std::string command)
{
	std::string receiver = command.substr(0, command.find(" "));

//	std::regex regex("\\w+\\s(.)*");
	std::regex regex("(([A-Z])|([a-z])|([0-9]))+\\s(.)*");
	std::smatch match;
	if (!std::regex_match(command, match, regex)){ // Invalid command structure.
		std::cout << SEND_FAILED << std::endl;
		return;
	}

	// The client try to send a message to himself.
	if (receiver.compare(client_name) == 0){
		std::cout << SEND_FAILED << std::endl;
		return;
	}

	// Create the request to the server.
	std::string request = SEND + command + END_LINE;

	if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
		close(sockfd);
		exit(1);
	}

	// Receive the server response.
	char msg [MAX_MSG_LEN];
	memset(msg, 0, sizeof(msg));
	client_recv_server_msg(msg);
}

/**
 * who operation, in a case the user want to know which clients are connect to the server now.
 * @param command - the reminder user input after the word "who"
 */
void client_who (std::string command)
{
	// Invalid command structure.
	if (command.compare("") != 0){
		std::cout << WHO_FAILED << std::endl;
		return;
	}

	// Create the request to the server.
	std::string request = WHO_COMMAND;
	if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
		std::cout << WHO_FAILED << std::endl;
		close(sockfd);
		exit(1);
	}

	// Receive the server response.
	char msg [MAX_MSG_LEN];
	memset(msg, 0, sizeof(msg));
	client_recv_server_msg(msg);
}

/**
 * exit operation, in a case the user want to disconnect from the server.
 * @param command - reminder from user input after the word "exit"
 */
void client_exit (std::string command)
{
	// Invalid command structure.
	if (command.compare("") != 0){
		std::cout << INVALID_COMMAND << std::endl;
		return;
	}

	// Create the request to the server.
	std::string request = EXIT_COMMAND;
	if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
		close(sockfd);
		exit(1);
	}

	// Receive the server response.
	char msg [MAX_MSG_LEN];
	memset(msg, 0, sizeof(msg));
	client_recv_server_msg(msg);

	close(sockfd);
	exit(0);
}

/**
 * This function receive a command (from the user) and send it to the treatment of cooresponding
 * funtion.
 * @param command the user command.
 */
void sent_command_to_function (std::string command)
{
	command = command.substr(0, command.find("\n"));
	std::string operation = command.substr(0, command.find(" "));

	if (operation.compare("create_group") == 0) {
		client_create_group(command.substr(command.find(" ") + 1));
	}
	else if (operation.compare("send") == 0) {
		client_send(command.substr(command.find(" ") + 1));
	}
	else if (operation.compare("who") == 0) {
//		std::string a = command.substr(command.find(" ") + 1);
		client_who(command.substr(3));
	}
	else if (operation.compare("exit") == 0) {
		client_exit(command.substr(4));
	}
	else {
		std::cout << INVALID_COMMAND << std::endl;
	}
}

/**
 * This function responsible to send the client name go the server after it connect to it.
 * @param name the client name.
 */
void send_name_to_server(std::string name){
	int bytes;
	std::string name_msg = "name " + name + END_LINE;
	if ((bytes = send(sockfd, name_msg.c_str(), name_msg.length(), 0)) < 0) {
		std::cout << "ERROR: send " << errno << "." << std::endl;
		close(sockfd);
		exit(1);
	}
}

/**
 * This function operate the select function - the client listen either to the server and to it's
 * stdin.
 */
void client_listen ()
{
	fd_set all_fds;
	fd_set read_fds;

	FD_ZERO(&all_fds);
	FD_ZERO(&read_fds);
	FD_SET(sockfd, &all_fds);
	FD_SET(STDIN_FILENO, &all_fds);

	int ret_val;
	while (true)
	{
		read_fds = all_fds;

		ret_val = select(sockfd + 1, &read_fds, NULL, NULL, NULL);

		if (ret_val < 0) // System call error
		{
			std::cout << "ERROR: select " << errno << "." << std::endl;
			close(sockfd);
			exit(1);
		}

		if (FD_ISSET(sockfd, &read_fds)) { // The server wrote something to me
			char msg [MAX_MSG_LEN];
			memset(msg, 0, sizeof(msg));
			client_recv_server_msg(msg);
		}
		else if (FD_ISSET(STDIN_FILENO, &read_fds)) { // User commands.
			std::string command;
			getline(std::cin, command);
			sent_command_to_function(command);
		}
	}
}


/**
 * The main function responsible to run the whole flow of the client side.
 * @param argc the number of arguments.
 * @param argv the arguments of the program (client name, address to connect to, port number).
 * @return
 */
int main (int argc, char *argv[])
{
	// Validity check
	if (argc != VALID_ARG_NUM) {
		std::cout << INVALID_ARG;
		return 0;
	}

	struct sockaddr_in server_address;
	struct hostent* host;

	memset(&server_address, 0, sizeof(server_address));

	if ((host = gethostbyname(argv[2])) == NULL){
		std::cout << "ERROR: gethostbyname " << errno << "." << std::endl;
	}

	// server_address initialization.
	server_address.sin_family = host->h_addrtype;
	memcpy(&server_address.sin_addr, host->h_addr, host->h_length);
	server_address.sin_port = htons(atoi(argv[3]));

	// Create the client socket.
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "ERROR: socket " << errno << "." << std::endl;
		exit(1);
	}
	// Connect to server.
	int con;
	if ((con = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address))) < 0) {
		std::cout << "ERROR: connect " << errno << "." << std::endl;
		std::cout << CON_FAIL << std::endl;
		close(sockfd);
		exit(1);
	}

	send_name_to_server(argv[1]);
	client_name = argv[1];

	// need to know if the connection success or not...
	char msg [MAX_MSG_LEN];
	memset(msg, 0, sizeof(msg));
	client_recv_server_msg(msg);

	std::string server_response(msg);
	std::string checkMsg = CON_FAIL + '\n';
	if (server_response.compare(CATCH_NAME) == 0 || server_response.compare(checkMsg) == 0){
		close(sockfd);
		exit(1);
	}

	client_listen();

	close(sockfd);
	return 0;
}
