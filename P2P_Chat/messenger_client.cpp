/*
	Neet Patel
	COP 5570
	Project 2: A Messenger Application
	12/04/2018

	messenger_client.cpp
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>

const static int PORT = 5100;
const static int MAXBUFLEN = 1041; // maximum message length (to clients) will be 1024 but +1 for the null character

typedef std::string string;
typedef std::list<string> slist;
typedef std::unordered_map<int, struct connected_friend> icf_umap;
typedef std::unordered_map<string, struct friend_information> sfi_umap;

struct connected_friend { // struct to hold username and thread id
	string username;
	pthread_t thread_id;
};

struct friend_information { // struct to hold friend information
	int fd;
	string ip;
	string port;
};

int serv_sockfd, sockfd, serv_port, port;
string serv_host, my_username;
bool pre_login = true;

icf_umap fd_cfriend;		// maps fd -> connected_friend structure ; inverse of the below mapping
sfi_umap funame_fdloc;		// maps friend username -> friend_information structure

pthread_mutex_t access_container_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pre_login_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pre_login_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t logout_cond = PTHREAD_COND_INITIALIZER;
// Threads for the client tasks.
// Main thread handles stdin
// A thread to handle server socket to obtain client location information
// A thread to handle local socket to accept client connects.
// A thread for each single accepted client connection. Spawnned from the above thread that accepts client connections
pthread_t sock_thread, serv_thread;

void *process_server(void *arg);	// server thread function
void *process_sock(void *arg);		// socket thread function
void *process_friend(void *arg);	// friend thread function

void sig_intrpt(int signo);			// SIGINT handler
void list_online_friends(void);		// lists current online friends
void close_friend_socks(void);		// closes friend sockets
/***********************************************************************************************************************
                                                        MAIN
***********************************************************************************************************************/
int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: ./messenger_client configuration_file\n");
		fprintf(stderr, "The 'configuration_file' must be in the same directory\n");
		exit(EXIT_FAILURE);
	}

	struct sigaction abc;
	abc.sa_handler = sig_intrpt;
	sigemptyset(&abc.sa_mask);
	abc.sa_flags = 0;
	sigaction(SIGINT, &abc, NULL);
	std::ifstream config_if(argv[1]);
	if (config_if.is_open()) { // read configuration file
		std::string line;
		std::getline(config_if, line, ':');
		config_if >> serv_host;
		std::getline(config_if, line, ':');
		config_if >> serv_port;
	} else {
		fprintf(stderr, "Failed to open 'configuration_file: %s'\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (argc == 3) {
		port = atoi(argv[2]);
	} else {
		port = PORT;
	}
	//printf("serv_host: %s, serv_port: %d\n", serv_host.c_str(), serv_port);
	// obtain server IP and Port
	struct addrinfo hints, *res, *ressave;
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int rv, flag;
	if ((rv = getaddrinfo(serv_host.c_str(), std::to_string(serv_port).c_str(), &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}
	ressave = res;
	// start of the client interface
	while (1) {
		flag = 0;
		do {
			serv_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			if (serv_sockfd < 0) {
				continue;
			}
			if (connect(serv_sockfd, res->ai_addr, res->ai_addrlen) == 0) {
				flag = 1;
				break;
			}
			close(serv_sockfd);
		} while ((res = res->ai_next));
		if (flag == 0) {
			fprintf(stderr, "Cannot connect to server socket. Server likely closed.\n");
			close(serv_sockfd);
			exit(EXIT_FAILURE);
		}
		// create the server listening thread
		if (pthread_create(&serv_thread, nullptr, process_server, nullptr) != 0) {
			printf("pthread_create: process server: %s\n", strerror(errno));
			close(serv_sockfd);
			exit(EXIT_FAILURE);
		}
		char buf[MAXBUFLEN];
		string op, field1, field2, discard;
		// stdin loop
		while (1) {
			printf("Please enter: r (register), l (login), exit (quit client)\n");
			//std::cin >> op;
			//std::getline(std::cin, discard); // make sure to discard newline or garbage input
			op.clear();
			field1.clear();
			field2.clear();
			std::getline(std::cin, op);
			// handle pre login phase
			if (op == "r" || op == "l") {
				string send_msg;
				printf("username (limited to 12 characters, no spaces): ");
				std::getline(std::cin, field1);
				printf("password (limited to 12 characters, no spaces): ");
				std::getline(std::cin, field2);
				if (field1.length() > 12 || field2.length() > 12) {
					printf("username and/or password length exceeds 12. Please limit your input to 12 characters for both fields.\n");
					continue;
				}
				if (field1.find(" ") != string::npos || field2.find(" ") != string::npos) {
					printf("username and/or password contains spaces. No spaces are allowed for the fields.\n");
					continue;
				}
				if (field1.empty() || field2.empty()) {
					printf("username and/or password fields empty.\n");
					continue;
				}
				if (op == "l") {
					my_username = field1;
				}
				sprintf(buf, "%s %s %s", op.c_str(), field1.c_str(), field2.c_str());
				write(serv_sockfd, buf, MAXBUFLEN - 1);
				// wait on server reply to see whether we can continue to post login phase
				pthread_mutex_lock(&pre_login_mutex);
				pthread_cond_wait(&pre_login_cond, &pre_login_mutex);
				if (pre_login) {
					pthread_mutex_unlock(&pre_login_mutex);
					continue;
				}
				pthread_mutex_unlock(&pre_login_mutex); // logged in successfully at this point
			} else if (op == "exit") { // exit client program
				close(serv_sockfd);
				exit(EXIT_SUCCESS);
			} else {
				if (!op.empty()) {
					printf("Command not recognized.\n");
				}
				continue;
			}
			// initialization complete and user logged in; create the local socket thread
			if (pthread_create(&sock_thread, nullptr, process_sock, nullptr) != 0) {
				printf("pthread_create: process sock: %s\n", strerror(errno));
				close(serv_sockfd);
				exit(EXIT_FAILURE);
			}
			// handle after logged in stdin
			while (1) {
				op.clear();
				field1.clear();
				field2.clear();
				std::getline(std::cin, discard);
				std::stringstream ss(discard);
				std::getline(ss, op, ' ');
				std::getline(ss, field1, ' ');
				std::getline(ss, field2);
				if (op == "m") {
					if (field1.length() == 0 || field1.length() > 12) {
						printf("Friend username not provided or exceeds 12 characters.\n");
						continue;
					}
					if (field2.length() == 0 || field2.length() > 1024) {
						printf("No message provided or message exceeds 1024 characters.\n");
						continue;
					}
					pthread_mutex_lock(&access_container_mutex);
					sfi_umap::iterator itr = funame_fdloc.find(field1);
					if (itr == funame_fdloc.end()) { // friend not logged in or is not a friend
						printf("%s is not online, OR is not in your friends list,\n", field1.c_str());
						printf("OR there is an error with username provided. Please check the friend username again.\n");
						pthread_mutex_unlock(&access_container_mutex);
						continue;
					}
					if (itr->second.fd == -1) { // connection not established so establish it
						int fsockfd;
						struct addrinfo hints, *res, *ressave; // obtain server IP and Port
						bzero(&hints, sizeof(struct addrinfo));
						hints.ai_family = AF_INET;
						hints.ai_socktype = SOCK_STREAM;
						int rv;
						//printf("getting addr for %s, %s\n", itr->second.ip.c_str(), itr->second.port.c_str());
						if ((rv = getaddrinfo(itr->second.ip.c_str(), itr->second.port.c_str(), &hints, &res)) != 0) {
							fprintf(stderr, "friend getaddrinfo error: %s\n", gai_strerror(rv));
							pthread_mutex_unlock(&access_container_mutex);
							continue;
						}
						ressave = res;
						if ((fsockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
							freeaddrinfo(ressave);
							close(fsockfd);
							fprintf(stderr, "friend socket error: %s\n", strerror(errno));
							pthread_mutex_unlock(&access_container_mutex);
							continue;
						}
						if (connect(fsockfd, res->ai_addr, res->ai_addrlen) != 0) {
							freeaddrinfo(ressave);
							close(fsockfd);
							fprintf(stderr, "friend connect error: %s\n", strerror(errno));
							pthread_mutex_unlock(&access_container_mutex);
							continue;
						}
						freeaddrinfo(ressave);
						itr->second.fd = fsockfd;
						sprintf(buf, "%s %s", my_username.c_str(), field2.c_str());
						write(fsockfd, buf, MAXBUFLEN - 1);
						pthread_t ftid;
						pthread_create(&ftid, nullptr, &process_friend, (void *)&fsockfd);
						struct connected_friend cf = {field1, ftid};
						fd_cfriend[fsockfd] = cf;
					} else {
						snprintf(buf, MAXBUFLEN - 1, "%s", field2.c_str());
						write(itr->second.fd, buf, MAXBUFLEN - 1);
					}
					pthread_mutex_unlock(&access_container_mutex);
				} else if (op == "i") {
					if (field1.length() == 0 || field1.length() > 12) {
						printf("Friend username not provided or exceeds 12 characters.\n");
						continue;
					}
					if (field2.length() > 1024) {
						printf("Message exceeds 1024 characters.\n");
						continue;
					}
					sprintf(buf, "i %s %s", field1.c_str(), field2.c_str());
					write(serv_sockfd, buf, MAXBUFLEN - 1);
				} else if (op == "ia") {
					if (field1.length() == 0 || field1.length() > 12) {
						printf("Friend username not provided or exceeds 12 characters.\n");
						continue;
					}
					if (field2.length() > 1024) {
						printf("Message exceeds 1024 characters.\n");
						continue;
					}
					sprintf(buf, "ia %s %s", field1.c_str(), field2.c_str());
					write(serv_sockfd, buf, MAXBUFLEN - 1);
				} else if (op == "logout") {
					sprintf(buf, "logout");
					write(serv_sockfd, buf, MAXBUFLEN - 1);
					pthread_mutex_lock(&access_container_mutex);
					close_friend_socks();
					pthread_mutex_unlock(&access_container_mutex);
					close(sockfd);
					pthread_cancel(sock_thread);
					break;
				} else if (op == "h") {
					printf("m  friend_username message\n");
					printf("i  invitee_username [message]\n");
					printf("ia inviter_username [message]\n");
					printf("logout\n");
				} else {
					if (!op.empty()) {
						printf("Command not recognized. Please try again or enter 'h' for help.\n");
					}
				}
			}
			// close friend threads
			for (auto fd_cf_itr = fd_cfriend.begin(); fd_cf_itr != fd_cfriend.end(); ++fd_cf_itr) {
				pthread_cancel(fd_cf_itr->second.thread_id);
			}
			// wait for server thread to signal so we know write went through
			pthread_mutex_lock(&logout_mutex);
			pthread_cond_wait(&logout_cond, &logout_mutex);
			close(serv_sockfd);
			pthread_mutex_unlock(&logout_mutex);
			pre_login = true;
			break;
		}
	}
	freeaddrinfo(ressave);
	exit(EXIT_SUCCESS);
}
/***********************************************************************************************************************
				Function defintions
***********************************************************************************************************************/
void *process_sock(void *arg) {
	pthread_detach(pthread_self());
	int n;
	struct addrinfo hints, *res, *ri;
	char hostname[1024];
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	hostname[1024 - 1] = '\0';
	gethostname(hostname, 1024 - 1);
	//printf("local hostname is: %s\n", hostname);
	if ((n = getaddrinfo(hostname, std::to_string(port).c_str(), &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n));

		exit(EXIT_FAILURE);
	}
	ri = res;
	getnameinfo(ri->ai_addr, ri->ai_addrlen, hostname, sizeof (hostname), nullptr, 0, NI_NUMERICHOST);
	freeaddrinfo(res);
	char buf[MAXBUFLEN];
	socklen_t len;
	struct sockaddr_in addr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "socket: can't get socket: %s", strerror(errno));
		close(sockfd);
		close(serv_sockfd);
		exit(EXIT_FAILURE);
	}
	int sockop = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sockop, sizeof(sockop))) {
		fprintf(stderr, "setsockopt: %s\n", strerror(errno));
		close(sockfd);
		close(serv_sockfd);
		exit(EXIT_FAILURE);
	}
	addr.sin_addr.s_addr = inet_addr(hostname);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "bind error: %s\n", strerror(errno));
		close(sockfd);
		close(serv_sockfd);
		exit(EXIT_FAILURE);
	}
	len = sizeof(addr);
	if (getsockname(sockfd, (struct sockaddr *)&addr, &len) < 0) {
		fprintf(stderr, "getsockname : can't get name: %s\n", strerror(errno));
		close(sockfd);
		close(serv_sockfd);
		exit(EXIT_FAILURE);
	}
	if (listen(sockfd, 5) < 0) {
		fprintf(stderr, "listen error: %s", strerror(errno));
		close(sockfd);
		close(serv_sockfd);
		exit(EXIT_FAILURE);
	}
	// send location info
	//printf("sending to server loc: %s %d\n", inet_ntoa(addr.sin_addr), port);
	sprintf(buf, "cl %s %d", inet_ntoa(addr.sin_addr), port);
	write(serv_sockfd, buf, MAXBUFLEN - 1);
	fd_cfriend.clear();
	funame_fdloc.clear();
	struct sockaddr_in recaddr;
	int rec_sock;
	string funame, message;
	while (1) { // accept connections
		if ((rec_sock = accept(sockfd, (struct sockaddr *)(&recaddr), &len)) < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EBADF) { // incase accept returns but socket fd is closed
				pthread_exit(nullptr);
			}
			pthread_mutex_lock(&access_container_mutex);
			fprintf(stderr, "friend accept error: %s", strerror(errno));
			close(rec_sock);
			close(serv_sockfd);
			close_friend_socks();
			pthread_mutex_unlock(&access_container_mutex);
			close(sockfd);
			exit(EXIT_FAILURE);
		}
		pthread_mutex_lock(&access_container_mutex);
		pthread_t ftid;
		//printf("accepted connection for addr %s\n", inet_ntoa(recaddr.sin_addr));
		string addr = string(inet_ntoa(recaddr.sin_addr));
		// local testing; accept gives 127.0.0.1 when using inet_ntoa but clients send 127.0.1.1
		if (addr == "127.0.0.1") {
			addr = "127.0.1.1";
		}
		// read the username and message; update data structures; start a friend thread
		read(rec_sock, buf, MAXBUFLEN - 1);
		std::stringstream ss(buf);
		std::getline(ss, funame, ' ');
		std::getline(ss, message);
		funame_fdloc[funame].fd = rec_sock;
		printf("%s >> %s\n", funame.c_str(), message.c_str());
		pthread_create(&ftid, nullptr, &process_friend, (void *)&rec_sock);
		struct connected_friend cf = {funame, ftid};
		fd_cfriend[rec_sock] = cf;
		pthread_mutex_unlock(&access_container_mutex);

	}
}

void *process_server(void *arg) {
	pthread_detach(pthread_self());
	int n;
	char buf[MAXBUFLEN];
	string reply;
	while (1) {
		n = read(serv_sockfd, buf, MAXBUFLEN - 1);
		if (n <= 0) {
			if (n == 0) {
				pthread_mutex_lock(&access_container_mutex);
				printf("Server closed. Exiting client.\n");
			} else {
				if (errno == EINTR) {
					continue;
				}
				if (errno == EBADF) { // incase accept returns but server socket fd is closed
					pthread_exit(nullptr);
				}
				pthread_mutex_lock(&access_container_mutex);
				printf("Something is wrong with server. Exiting client.\n");
			}
			close(serv_sockfd);
			close(sockfd);
			close_friend_socks();
			pthread_mutex_unlock(&access_container_mutex);
			exit(EXIT_FAILURE);
		}
		buf[n] = '\0';
		std::stringstream ss(buf);
		string option, field1, field2, field3;
		std::getline(ss, option, ' ');
		std::getline(ss, field1, ' ');
		if (option == "r" || option == "l") {
			pthread_mutex_lock(&pre_login_mutex);
			std::getline(ss, field2);
			printf("%s\n", field2.c_str());
			if (field1 == "200" && option == "l") {
				pre_login = false; // registration successful so user must login in now
				printf("You can now enter chat commands or press 'h' for help.\n");
			}
			pthread_cond_signal(&pre_login_cond);
			pthread_mutex_unlock(&pre_login_mutex);
		} else if (option == "fl") { // friend location information ; update the maps
			pthread_mutex_lock(&access_container_mutex);
			do {
				std::getline(ss, field2, ' ');
				std::getline(ss, field3, ':');
				//pthread_mutex_lock(&access_container_mutex);
				struct friend_information fi = { -1, field2, field3};
				funame_fdloc[field1] = fi;
				field1.clear();
				std::getline(ss, field1, ' ');
				//list_online_friends();
			} while (!field1.empty());
			list_online_friends();
			pthread_mutex_unlock(&access_container_mutex);
		} else if (option == "i") {
			std::getline(ss, field2);
			printf("%s invited you and said: %s\n", field1.c_str(), field2.c_str());
		} else if (option == "ia") {
			std::getline(ss, field2);
			printf("%s accepted your invitation and said: %s\n", field1.c_str(), field2.c_str());
		} else if (option == "fc") {
			pthread_mutex_lock(&access_container_mutex);
			sfi_umap::iterator fu_fdloc_itr = funame_fdloc.find(field1);
			if (fu_fdloc_itr == funame_fdloc.end()) { // friend thread handled the closing of connection already
				pthread_mutex_unlock(&access_container_mutex);
				continue;
			}
			printf("%s is offline. Closing connection.\n", field1.c_str());
			close(fu_fdloc_itr->second.fd);
			fd_cfriend.erase(fu_fdloc_itr->second.fd);
			funame_fdloc.erase(field1);
			list_online_friends();
			pthread_mutex_unlock(&access_container_mutex);
		} else if (option == "lo") {
			pthread_mutex_lock(&logout_mutex);
			pthread_cond_signal(&logout_cond);
			pthread_mutex_unlock(&logout_mutex);
			pthread_exit(nullptr);
		} else { // likely an error message
			printf("%s\n", buf);
			continue;
		}
	}
}

void *process_friend(void *arg) {
	pthread_detach(pthread_self());
	int n;
	int ffd = *(int *)arg;
	char buf[MAXBUFLEN];
	string uname;
	while (1) {
		n = read(ffd, buf, MAXBUFLEN - 1);
		if (n <= 0) {
			pthread_mutex_lock(&access_container_mutex);
			icf_umap::iterator fd_cf_itr = fd_cfriend.find(ffd);
			if (fd_cf_itr == fd_cfriend.end()) { // fc notification from server already deleted information
				pthread_mutex_unlock(&access_container_mutex);
				pthread_exit(nullptr);
			}
			if (n == 0) { // friend closed
				printf("%s is offline. Closing connection.\n", fd_cfriend[ffd].username.c_str());
			} else {
				if (errno == EINTR) {
					pthread_mutex_unlock(&access_container_mutex);
					continue;
				}
				if (errno == EBADF) { // incase accept returns but friend socket fd is closed
					pthread_mutex_unlock(&access_container_mutex);
					pthread_exit(nullptr);
				}
				printf("Something is wrong with %s's connection, so client is closing the connection.\n", fd_cfriend[ffd].username.c_str());
			}
			// close socket and erase information from related data structures and exit thread.
			close(ffd);
			uname = fd_cfriend[ffd].username;
			fd_cfriend.erase(ffd);
			funame_fdloc.erase(uname);
			list_online_friends();
			pthread_mutex_unlock(&access_container_mutex);
			pthread_exit(nullptr);

		}
		buf[n] = '\0';
		pthread_mutex_lock(&access_container_mutex);
		uname = fd_cfriend[ffd].username;
		pthread_mutex_unlock(&access_container_mutex);
		printf("%s >> %s\n", uname.c_str(), buf);
	}
}
/***********************************************************************************************************************
				Utility function definitions
***********************************************************************************************************************/
void sig_intrpt(int signo) {
	close(sockfd);
	printf("\nInterrupt caught. Closed local socket...\n");
	close(serv_sockfd);
	printf("Closed server socket...\n");
	close_friend_socks();
	printf("Closed friends' sockets...\n");
	exit(EXIT_SUCCESS);
}

void close_friend_socks(void) {
	for (auto itr = fd_cfriend.begin(); itr != fd_cfriend.end(); ++itr) {
		close(itr->first);
	}
}

void list_online_friends(void) {
	printf("Friends online: ");
	auto fitr = funame_fdloc.begin();
	for (; fitr != funame_fdloc.end(); ) {
		printf("%s", fitr->first.c_str());
		if (++fitr != funame_fdloc.end()) {
			printf(", ");
		}
	}
	printf("\n");
}