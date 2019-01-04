--------------------------------------
| Project 2: A Messenger application |
--------------------------------------
This a P2P messenger application which uses BSD sockets. There is a server program which facilitates initial client
connections, and forwarding of necessary client location information to client's friends. Clients can register with
server and then log in. Once logged in, they can directly send messages to their online friends without relaying it
to the server. The clients can also invite other users to be their friends and the other invitee can simply ignore
the invitation for the session by doing nothing or accept it per the commands outlined in specification.

NOTE: The client program can take as its second command line argument a port number if port 5100 is not available. This
is how the testing was done as well.

-------------
| COMPILING |
-------------
- to compile both messenger_server and messenger_client:
  $ make all
  OR
  $ make

- to compile individual programs:
  $ make meessenger_server
  $ make messenger_client

- to clean directory:
  $ make clean


-------------------
| IO Multiplexing |
-------------------
Server:
	Uses select() system call.

Client:
	Uses pthreads:
		1) The main thread handles stdin
		2) A pthread handles server socket
		3) A pthread handles client socket
		4) A pthread is spawned, for each client to client connection, by the thread handling client socket.

------------
| Protocol |
------------
Client --> Server
-----------------
	In main thread:
		 r {username} {password}							            // register information
		 l {username} {password}							            // login information

		cl {ip} {port}										                // client location information
		 i {potential_friend_username} [whatever_message]	// client inviting a user
		ia {inviter_username} [whatever_message]			    // client accepting user invitation
		logout

Server --> Client
-----------------
	Register messages:
		r 500 Username already occupied. Please enter a different username.
		r 200 Registration successful. You can login in now.

	Login messages:
		l 500 Username not found.
		l 500 Username and password do not match. Please try again.
		l 500 User already logged in.
		l 200 Login successful. You are now online.

	Location:
		fl {username} {ip} {port}:{username} {ip} {port}:....	// exchange location info of online friends
																                             the just logged in user receives a chained format
																                             as shown
		 i {inviter_username} [message] 						          // forward invitation
		ia {invitee_username} [message]							          // invitation accepted by user
		fc {username} {message}									              // server informs that user that friend has logged out
		lo 														                        // server confirms to user that they are logged out



Client --> Client
-----------------
	m {username} [message]	// user sending message to a friend on a established connection


---------------
| Limitations |
---------------
1) Messeages to clients limited to 1024 characters for stability and reliability. Programs check for this.
2) A single registered user can only have one active logged in session at anytime. The server checks for this.
3) Any system call failures that shouldn't occur typically (eg. accept() call), will close the respective client/server
   program properly.
4) Server erases all invite/invite accept related information once a user logs out. The inviter will have to resend
   the invite.
4) Currently, Standard ouput will be interleaved if in a client something is printed to the screen whilst user is typing
   someting.
