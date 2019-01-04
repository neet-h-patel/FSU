/*
	Neet Patel
	COP 5570
	Project 2: A Messenger Application
	12/04/2018

	messenger_server.cpp
*/
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>

const static int MAXBUFLEN = 1041;

char buf[MAXBUFLEN]; // global buffer since server doesn't multithread;
typedef std::string string;
typedef std::list<string> slist;
typedef std::unordered_map<string, slist> sls_umap;
typedef std::unordered_map<int, slist> ils_umap;
typedef std::unordered_map<string, int> si_umap;
typedef std::unordered_multimap<string, string> ss_ummap;

sls_umap user_info; 		// maps user -> password,friends

ils_umap fd_userloc;		// maps fd -> list of username, ip, port
si_umap user_fd;			// maps logged in username -> fd; checking this lets us know users online
ss_ummap inviter_invitee; 	// maps inviter username -> invitee username
ss_ummap invitee_inviter;	// maps invitee username -> inviter username

int sockfd, port;
string option, field1, field2;
string send_msg;
string user_info_file;

void sig_intrpt(int signo);
void print_server_info(void);
void handle_register(int fd);
void handle_login(int fd); 		// upon successful login, will map the fd -> username
void handle_location_info(int fd);
void handle_invite(int fd);
void handle_invite_accept(int fd);

void handle_logout_exit(int fd, ils_umap::iterator& fditr, fd_set& set);
void update_user_friends(const string& cuser); // updates friends of cuser that cuser is logged out
void save_user_info(void);
/***********************************************************************************************************************
                                                        MAIN
***********************************************************************************************************************/
int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: ./messenger_server user_info_file configuration_file\n");
		fprintf(stderr, "The 'user_info_file' and 'configuration_file' must be in the same directory\n");
		exit(EXIT_FAILURE);
	}

	struct sigaction abc;
	abc.sa_handler = sig_intrpt;
	sigemptyset(&abc.sa_mask);
	abc.sa_flags = 0;
	sigaction(SIGINT, &abc, NULL);

	user_info_file = argv[1];
	std::ifstream user_if(argv[1]);
	if (user_if.is_open()) {
		std::string line;
		string user;
		string infos;
		while (std::getline(user_if, line)) {
			std::stringstream ss(line);
			std::getline(ss, user, '|');
			std::getline(ss, infos, '|');
			user_info[user].push_back(infos); // insert password
			while (std::getline(ss, infos, ';')) {
				user_info[user].push_back(infos); // insert friends for user
			}
		}

	} else {
		fprintf(stderr, "Failed to open 'user_info_file: %s'\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	user_if.close();
	std::ifstream config_if(argv[2]);
	if (config_if.is_open()) {
		std::string line;
		std::getline(config_if, line, ':');
		config_if >> port;
	} else {
		fprintf(stderr, "Failed to open 'configuration_file: %s'\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	config_if.close();
	//printf("server port is: %d\n", port);

	socklen_t len;
	struct sockaddr_in addr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "socket: can't get socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	int sockop = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sockop, sizeof(sockop))) {
		fprintf(stderr, "setsockopt: %s\n", strerror(errno));
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, ": bind");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	len = sizeof(addr);
	if (getsockname(sockfd, (struct sockaddr *)&addr, &len) < 0) {
		fprintf(stderr, "getsockname : can't get name: %s\n", strerror(errno));
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	port = ntohs(addr.sin_port);
	print_server_info();
	if (listen(sockfd, 5) < 0) {
		perror(": bind");
		exit(EXIT_FAILURE);
	}
	fd_set allset, rset;
	int maxfd;
	FD_ZERO(&allset);
	FD_SET(sockfd, &allset);
	maxfd = sockfd;
	fd_userloc.clear();
	string res;
	struct sockaddr_in recaddr;
	int rec_sock;
	while (1) { // select loop
		rset = allset;
		select(maxfd + 1, &rset, NULL, NULL, NULL);
		if (FD_ISSET(sockfd, &rset)) { // somebody tries to connect so accept
			if ((rec_sock = accept(sockfd, (struct sockaddr *)(&recaddr), &len)) < 0) {
				if (errno == EINTR) {
					continue;
				}
				fprintf(stderr, "accept: %s", strerror(errno));
				close(rec_sock);
				close(sockfd);
				exit(EXIT_FAILURE);

			}
			// client info
			printf("client successfully connected. ip: %s, port: %d\n", inet_ntoa(recaddr.sin_addr), ntohs(recaddr.sin_port));
			fd_userloc[rec_sock];
			FD_SET(rec_sock, &allset);
			if (rec_sock > maxfd) {
				maxfd = rec_sock;
			}
		}
		int num, fd;
		// check if any client socket is ready to be read from
		for (auto itr = fd_userloc.begin(); itr != fd_userloc.end(); /*++itr*/) {
			fd = itr->first;
			if (FD_ISSET(fd, &rset)) {
				if ((num = read(fd, buf, MAXBUFLEN - 1)) < 0) {
					if (errno == EINTR) {
						continue;
					}
					fprintf(stderr, "read error client fd: %s\n", strerror(errno));
					continue;
				}
				if (num == 0) { // client exited; close fds; let friends know; display total online users
					handle_logout_exit(fd, itr, allset);
					continue;
				}
				buf[num] = '\0';
				std::stringstream ss(buf);
				std::getline(ss, option, ' ');
				std::getline(ss, field1, ' ');
				std::getline(ss, field2);
				if (option == "r") {
					handle_register(fd);
				} else if (option == "l") {
					handle_login(fd);
				} else if (option == "cl") {
					handle_location_info(fd);
				} else if (option == "i") {
					handle_invite(fd);
				} else if (option == "ia") {
					handle_invite_accept(fd);
				} else if (option == "logout") { // same as client exit but with logged in user
					handle_logout_exit(fd, itr, allset);
					continue;
				} else {
					send_msg = "500 Command not recongized";
				}
			}
			++itr;
		}
		maxfd = sockfd;
		if (!fd_userloc.empty()) {
			int fd;
			for (auto itr = fd_userloc.begin(); itr != fd_userloc.end(); ++itr) {
				fd = itr->first;
				if (fd > maxfd) {
					maxfd = fd;
				}
			}
		}
	}
}
/***********************************************************************************************************************
												Function defintions
***********************************************************************************************************************/
void handle_register(int fd) {
	sls_umap::iterator user = user_info.find(field1);
	if (user == user_info.end()) {
		user_info[field1].push_back(field2);
		printf("Registered user: %s\n", field1.c_str());
		sprintf(buf, "r 200 Registration successful. You can login in now.");
	} else {
		sprintf(buf, "r 500 Username already occupied. Please enter a different username.");
	}
	write(fd, buf, MAXBUFLEN - 1);
}

void handle_login(int fd) {
	sls_umap::iterator user = user_info.find(field1);
	if (user == user_info.end()) {
		//send_msg = "500 Username not found.";
		sprintf(buf, "l 500 Username not found.");
	} else if (user->second.front() != field2) {
		sprintf(buf, "l 500 Username and password do not match. Please try again.");
	} else if (user_fd.find(field1) != user_fd.end()) {
		sprintf(buf, "l 500 User is already logged in.");
	} else {
		// map username -> fd
		user_fd[field1] = fd;
		// add the user name as the first element in the location list
		fd_userloc[fd].push_back(field1);
		printf("Logged in user: %s\n", fd_userloc[fd].front().c_str());
		printf("Users online: %ld\n", user_fd.size());
		fflush(stdout);
		sprintf(buf, "l 200 Login successful. You are now online.");
	}
	write(fd, buf, MAXBUFLEN - 1);
}

void handle_location_info(int fd) {
	// insert user location information
	slist& user_loc_list = fd_userloc[fd];
	user_loc_list.push_back(field1);
	user_loc_list.push_back(field2);
	//printf("%s connected with ip:%s, port:%s\n", user_loc_list.front().c_str(), field1.c_str(), field2.c_str());
	// get user's friends list and exchange location info between them (if friend is online)
	const slist& flist = user_info[user_loc_list.front()];
	string fip, fport;
	string to_user = "fl ";
	for (auto fitr = ++(flist.begin()); fitr != flist.end(); ++fitr) {
		si_umap::iterator flog_itr = user_fd.find(*fitr);
		if (flog_itr == user_fd.end()) { // friend is not logged in
			continue;
		}
		// friend logged in so exchange info
		const slist& floc_list = fd_userloc.find(flog_itr->second)->second;
		slist::const_iterator floc_itr = ++floc_list.begin();
		if (floc_itr != floc_list.end()) {
			// send friend info to user
			fip = *floc_itr++;
			fport = *floc_itr;
			to_user += *fitr + " " + fip + " " + fport + ":";
			// sprintf(buf, "fl %s %s %s", (*fitr).c_str(), fip.c_str(), fport.c_str());
			// write(fd, buf, MAXBUFLEN - 1);
			// send user info to friend
			sprintf(buf, "fl %s %s %s", user_loc_list.front().c_str(), field1.c_str(), field2.c_str());
			write(flog_itr->second, buf, MAXBUFLEN - 1);
			continue;
		}
	}
	if (to_user.length() > 3) {
		sprintf(buf, "%s", to_user.c_str());
		write(fd, buf, MAXBUFLEN - 1);
	}
}

void handle_invite(int fd) {
	// check if invitee is in database
	string inviter = fd_userloc[fd].front();
	sls_umap::iterator invitee_info_itr = user_info.find(field1);
	if (invitee_info_itr == user_info.end()) {
		sprintf(buf, "No such user, %s, in database!", field1.c_str());
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	// check if invitee is logged in
	si_umap::iterator invitee_log_itr = user_fd.find(field1);
	if (invitee_log_itr == user_fd.end()) {
		sprintf(buf, "%s is not logged in.", field1.c_str());
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	// check if invitee is already a friend
	slist& flist = user_info[inviter];
	for (auto fitr = ++flist.begin(); fitr != flist.end();) {
		if (*fitr == field1) {
			sprintf(buf, "%s is already your friend.", field1.c_str());
			write(fd, buf, MAXBUFLEN - 1);
			return;
		} else {
			if (++fitr == flist.end()) {
				break;
			}
		}
	}
	// check if request is to self
	if (inviter == field1) {
		sprintf(buf, "Cannot invite to self.");
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	// Check if request pending; just send it again without updating maps
	std::pair<ss_ummap::iterator, ss_ummap::iterator> pend_itr_pair = inviter_invitee.equal_range(inviter);
	for (auto itr = pend_itr_pair.first; itr != pend_itr_pair.second; ++itr) {
		if (itr->second == field1) {
			sprintf(buf, "i %s %s", inviter.c_str(), field2.c_str());
			write(user_fd[field1], buf, MAXBUFLEN - 1);
			return;
		}
	}
	// update the maps for request verification
	inviter_invitee.insert(std::pair<string, string>(inviter, field1));
	invitee_inviter.insert(std::pair<string, string>(field1, inviter));
	sprintf(buf, "i %s %s", inviter.c_str(), field2.c_str());
	write(user_fd[field1], buf, MAXBUFLEN - 1);
}

void handle_invite_accept(int fd) {
	string invitee = fd_userloc[fd].front();
	sls_umap::iterator inviter_info_itr = user_info.find(field1);
	if (inviter_info_itr == user_info.end()) {
		sprintf(buf, "No such user, %s, in database!", field1.c_str());
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	si_umap::iterator inviter_log_itr = user_fd.find(field1);
	if (inviter_log_itr == user_fd.end()) {
		sprintf(buf, "%s is not logged in.", field1.c_str());
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	slist& flist = user_info[invitee];
	for (auto fitr = ++flist.begin(); fitr != flist.end();) {
		if (*fitr == field1) {
			sprintf(buf, "%s is already your friend.", field1.c_str());
			write(fd, buf, MAXBUFLEN - 1);
			return;
		} else {
			if (++fitr == flist.end()) {
				break;
			}
		}
	}
	// check if request is to self
	if (invitee == field1) {
		sprintf(buf, "Cannot accept invitation to self.");
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	std::pair<ss_ummap::iterator, ss_ummap::iterator> pend_itr_pair = invitee_inviter.equal_range(invitee);
	bool no_invites = true;
	auto itr = pend_itr_pair.first;
	for (; itr != pend_itr_pair.second; ++itr) {
		if (itr->second == field1) { // request pending so just send it again without updating maps
			no_invites = false;
			break;
		}
	}
	if (no_invites) {
		sprintf(buf, "You have no invite from user %s.", field1.c_str());
		write(fd, buf, MAXBUFLEN - 1);
		return;
	}
	// forward to inviter
	sprintf(buf, "ia %s %s", invitee.c_str(), field2.c_str());
	write(user_fd[field1], buf, MAXBUFLEN - 1);
	// remove the mapping
	inviter_invitee.erase(field1);
	// update user information for both friends
	user_info[invitee].push_back(field1);
	user_info[field1].push_back(invitee);
	// exchange location info; send invitee info to inviter
	slist::iterator uloc_itr = ++fd_userloc[fd].begin();
	string ip = *uloc_itr++;
	string port = *uloc_itr;
	sprintf(buf, "fl %s %s %s", invitee.c_str(), ip.c_str(), port.c_str());
	write(inviter_log_itr->second, buf, MAXBUFLEN - 1);
	// send inviter info to invitee
	uloc_itr = ++fd_userloc[inviter_log_itr->second].begin();
	ip = *uloc_itr++;
	port = *uloc_itr;
	sprintf(buf, "fl %s %s %s", field1.c_str(), ip.c_str(), port.c_str());
	write(fd, buf, MAXBUFLEN - 1);
	// delete mappings; delete invitee -> inviter;
	invitee_inviter.erase(itr);
	// delete inviter -> invitee
	pend_itr_pair = inviter_invitee.equal_range(field1);
	for (itr = pend_itr_pair.first; itr != pend_itr_pair.second; ++itr) {
		if (itr->second == invitee) {
			inviter_invitee.erase(itr);
			break;
		}
	}
}

void handle_logout_exit(int fd, ils_umap::iterator& fditr, fd_set& set) {
	string cuser;
	if (!fditr->second.empty()) { // remove from logged in mapping if logged in
		cuser = fditr->second.front();
		user_fd.erase(cuser);
		// erase mapping from inviter -> invitee where cuser is an invitee
		auto mmitr = inviter_invitee.begin();
		for (; mmitr != inviter_invitee.end(); ) {
			if (mmitr->second == cuser) {
				mmitr = inviter_invitee.erase(mmitr);
			} else {
				++mmitr;
			}
		}
		// erase mapping from inviter -> invitee where inviter is cuser
		std::pair<ss_ummap::iterator, ss_ummap::iterator> itr_pair = inviter_invitee.equal_range(cuser);
		auto itr = itr_pair.first;
		for (; itr != itr_pair.second; ) {
			itr = inviter_invitee.erase(itr);
		}
		// erase mapping from invitee -> inviter where invitee is cuser
		itr_pair = invitee_inviter.equal_range(cuser);
		itr = itr_pair.first;
		for (; itr != itr_pair.second; ) {
			itr = invitee_inviter.erase(itr);
		}
		update_user_friends(cuser);
	}
	//fd_userloc[fd].clear();
	sprintf(buf, "lo");
	write(fd, buf, MAXBUFLEN - 1);
	close(fd);
	FD_CLR(fd, &set);
	fditr = fd_userloc.erase(fditr);
	printf("Client exited.\n");
	printf("Users online: %ld\n", user_fd.size());
}
/***********************************************************************************************************************
Utility function definitions
***********************************************************************************************************************/
void sig_intrpt(int signo) {
	printf("Interrupt caught. Closing server...\n");
	close(sockfd);
	printf("Closing clients\n"); //and letting them know...\n");
	//int fd;
	for (auto itr = fd_userloc.begin(); itr != fd_userloc.end(); ++itr) {
		close(itr->first);
	}
	save_user_info();
	exit(EXIT_SUCCESS);
}

void update_user_friends(const string& cuser) {
	const slist& flist = user_info[cuser];
	int ffd;
	for (auto fitr = ++flist.begin(); fitr != flist.end(); ++fitr) { // loop through user's friends list
		si_umap::iterator flog_itr = user_fd.find(*fitr);
		if (flog_itr == user_fd.end()) { // friend is not logged in
			continue;
		}
		ffd = user_fd[*fitr];
		sprintf(buf, "fc %s", cuser.c_str());
		write(ffd, buf, MAXBUFLEN - 1);
	}
}

void save_user_info(void) {
	std::ofstream user_of(user_info_file);
	if (user_of.is_open()) {
		for (auto itr = user_info.begin(); itr != user_info.end(); ++itr) {
			slist &friends_list = itr->second;
			user_of << itr->first << '|' << friends_list.front() << '|';
			for (auto it2 = ++(friends_list.begin()); it2 != friends_list.end(); ) {
				user_of << *it2;
				if (++it2 != friends_list.end()) {
					user_of << ';';
				} else {
					break;
				}
			}
			user_of << '\n';
		}
	} else {
		fprintf(stderr, "Failed to open 'user_info_file: %s'\n", strerror(errno));
		fprintf(stderr, "failed to save potential new information\n");
		exit(EXIT_FAILURE);
	}
	user_of.close();
}

void print_server_info(void) {
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
	printf("hostname(s): ");
	ri = res;
	for (ri = res; ri; ) {
		printf("%s", ri->ai_canonname);
		if ((ri = ri->ai_next)) {
			printf(", ");
		} else {
			printf("\n");
		}
	}
	freeaddrinfo(res);
	printf("port = %d\n", ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port));
}
