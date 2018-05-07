/* 
 * Server for Call Monitoring of Avaya Definity Systems
 * 	using service observing.
 * jsolares@codevoz.com
 */

#include <stdio.h>      
#include <errno.h>
#include <sys/socket.h> 
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <unistd.h>    
#include <event.h>
#include <signal.h>
#include "bstrlib.h"

/* Maximum Qty of Pending Requests */
#define MAXPENDING 10

/* Receiving Buffer Size */
#define RCVBUFFER  256

/* DEBUG */
#define DEBUG

/* Event Structures for handling SIGNALS, SIGTERM, SIGCHLD... */
struct event sigchldev, sigtermev;

/*
 * Listen Queue Structure
 * struct event ev : Event Structure for handling this client/server
 * int        sock : The Network Socket to monitor, read, write to
 * int       avaya : Define if this is an Avaya Client/Server 
 *                      (The Avaya Server (we) Handle the Avaya Client (definity) 
 *                      as well as call monitoring clients from asterisk)
 * struct listenq * next: The next Client/Server of the Queue
 *
 * clientq, current_cli    : Pointer to the First Client that connected, Pointer to the Last
 * server,  current_server : Same as above for our two Servers
 */
struct listenq;

struct listenq {
	struct event          ev;
	int                   sock;
	int					  avaya;
	int					  destroy;
	struct listenq*       next;
} *clientq, *current_cli, *server, *current_server;

/*
 * Extensions Captured Structure
 */
struct exten {
	struct event    ev;
	int             incoming;
	int             outgoing;
	int			    sock;
	struct listenq *c_client;
};

/*
 * The Counter for RDT Compliance. see below in avaya_server (...).
 */
char counter = 0;

/*
 * err_handler : Error Handling Function, char * : Message to display
 */
void err_handler (char * );

/*
 * avaya_server
 * The Function responsible for receiving the data sent by the Definity PBX for processing
 * It schedules a write to the Clients when it captures extensions.
 *
 * int     fd : Socket/File Descriptor that triggered the event
 * short   ev : Event Codes
 * void *data : Data we receive from the Event when it was first scheduled.
 */
void avaya_server (int fd, short ev, void *data); 

/*
 * client_handler
 * The Function responsible for communicating with our Asterisk Call Monitoring clients
 */
void client_handler ( int fd, short ev, void *data);

/*
 * net_accept
 * The Function responsible for accepting connections to our asterisk on our two ports
 * it adds clients to the clientqueue and sets up their event and resets the event of the server(s)
 */
static void net_accept (int fd, short ev, void *data);

/*
 * net_setup
 * The Function that starts the Servers, it listens on all interfaces on port `port`
 * and `avaya` defines if it's going to listen for the Definity PBX or write to the asterisk clients
 *
 * It sets up the event for monitoring the Server Socket
 */
void net_setup ( unsigned short port, int avaya );

/*
 * Signalling Function Called when there's a SIGTERM
 */
void sigterm (int sig, short ev, void *data);

/*
 * Signalling Function Called when there's a SIGCHLD
 */
void sigchld (int sig, short ev, void *data);

/*
 * signal_setup()
 * Function in charge of setting up the Signalling events.
 */
void signal_setup();

int main ( int argc, char** argv) {
	/*
	 * Initialization of the Client Queue for Connections
	 */
	clientq = malloc(sizeof(*clientq));
	clientq->next = NULL;
	clientq->destroy = 1;
	current_cli = clientq;

	/*
	 * Initialization of the Server Queue (2, Avaya Definity and Asterisk)
	 */
	server = malloc(sizeof(*server));
	server->next = NULL;
	current_server = server;
	
	/* Event Loop Initialization */
	event_init();

	/*
	 * Start up the Servers, Parameters : Port and AvayaFlag 
	 *                (1 if Definity Server, 0 if Asterisk)
	 */
	net_setup (9000,1);
	net_setup (9002,0);

	signal_setup();         /* Setup the Signal Handlers */
	event_dispatch();       /* Start the Event Loop      */

	return(1);
}

void signal_setup() {
	/* Set the SIGTERM Handler */
	signal_set(&sigtermev, SIGTERM, sigterm, NULL);
	if (signal_add(&sigtermev, NULL) == -1)
		err_handler ("signal_add()");

	/* Set the SIGCHLD Handler */
	signal_set(&sigchldev, SIGCHLD, sigchld, NULL);
	if (signal_add(&sigchldev, NULL) == -1)
		err_handler ("signal_add()");
}

void sigchld (int sig, short ev, void *data) {
	int status;
	pid_t pid;
	
	/* Kill all Children (Not needed since we dont fork() but might in the future */
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
			(pid < 0 && errno == EINTR));
}

void sigterm (int sig, short ev, void *data) {
	char *sigstr, message[256];
	switch (sig) {
		case SIGTERM:
			sigstr = "SIGTERM";
			break;
		case SIGINT:
			sigstr = "SIGINT";
			break;
		default:
			sigstr = "an unknown signal";
			break;
	}
	
	sprintf (message, "Received %s; quitting", sigstr);
	err_handler(message);
}

void net_setup ( unsigned short port, int avaya ) {
	int SocketSrv, on = 1;
	struct sockaddr_in Srv;
	struct listenq * srv_q;

	/* Listen on Any Interface and on Port `port` */
	memset(&Srv, 0, sizeof(Srv));
	Srv.sin_family = AF_INET;
	Srv.sin_addr.s_addr = htonl(INADDR_ANY);
	Srv.sin_port = htons(port);

	/* Start up the Socket */
	if (( SocketSrv = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		err_handler("socket() failed");

	/* Set Socket option SO_REUSEADDR, To be able to reuse the address */
	if (setsockopt (SocketSrv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
		err_handler("setsockopt() failed");

	/* Set our Socket to nonblocking mode to be event driven */
	if (fcntl(SocketSrv, F_SETFL, O_NONBLOCK) == -1)
		err_handler("fcntl() failed");
	
	/* Bind our Socket to the Address specified, INADDR_ANY */
	if (bind(SocketSrv, (struct sockaddr *) &Srv, sizeof(Srv)) < 0)
		err_handler("bind() failed");

	/* Start listening for Connections having MAXPENDING pending clients to handle */
	if (listen(SocketSrv, MAXPENDING) < 0)
		err_handler("listen() failed");

	/* Setup our Server Queue for this server */
	current_server->sock = SocketSrv;
	current_server->avaya = avaya;
	current_server->next = malloc(sizeof(*server));
	srv_q = current_server;
	current_server = current_server->next;
	current_server->next = NULL;

	/* DEBUG */
	#ifdef DEBUG
	fprintf ( stderr, "Creating Server %d to listen on port %d\n", SocketSrv, port );
	#endif

	/* Set the Event for handling the Clients */
	event_set (&srv_q->ev, srv_q->sock, EV_READ, net_accept, srv_q);
	if ( event_add (&srv_q->ev, NULL) == -1 )
		err_handler("event_add() - failed");
}

static void net_accept (int fd, short ev, void *data) {
	struct sockaddr cliaddr;
	socklen_t addrlen = sizeof(cliaddr);
	int clisock;
	struct listenq *serverq = (struct listenq *)data;
	struct listenq *cli_q;

	/* Accept the connection from Client `clisock` */
	if ((clisock = accept(serverq->sock, &cliaddr, &addrlen)) == -1)
		goto out;

	/* Set the Client's socket to nonblocking mode */
	if (fcntl(clisock, F_SETFL, O_NONBLOCK) == -1) 
		err_handler("fcntl() failed");

	/* Setup the Client Queue to handle our current client */
	current_cli->sock  = clisock;
	current_cli->avaya = serverq->avaya;
	current_cli->destroy = 0;
	current_cli->next  = malloc (sizeof(*current_cli));
	cli_q = current_cli;
	current_cli = current_cli->next;
	current_cli-> next = NULL;
	current_cli->destroy = 1;

	/* DEBUG */
	#ifdef DEBUG
	fprintf ( stderr, "Socket : %d, Server : %d\n", cli_q->sock, serverq->sock);
	#endif

	/*
	 * Handle both Cases, a connection from the Avaya Server and from Asterisk 
	 *    and setup the events for the clients. Which will trigger once the client
	 *    sends us data.
	 */
	if (serverq->avaya) {
		event_set ( &cli_q->ev, cli_q->sock, EV_READ, avaya_server, cli_q);
		if ( event_add ( &cli_q->ev, NULL ) == -1 )
			err_handler("event_add() 1 failed");
	} else {
		event_set ( &cli_q->ev, cli_q->sock, EV_READ, client_handler, cli_q);
		if ( event_add ( &cli_q->ev, NULL ) == -1 )
			err_handler("event_add() 2 failed");
	}

	/* DEBUG */
	#ifdef DEBUG
	fprintf ( stderr, "Readding Server Socket to Events\n");
	#endif

out:
	/* Add the event so we handle future connections */
	if ( event_add (&serverq->ev, NULL) == -1 )
		err_handler("event_add() 3 failed");
}

void client_handler ( int fd, short ev, void *data) {
	char buffer[RCVBUFFER + 1];

	if (ev & EV_READ) { /* Do we Read from our Clients or Not?? */
		struct listenq *client_q = (struct listenq *) data;

		/* DEBUG */
		#ifdef DEBUG
		fprintf ( stderr, "Entering Client Handler for %d\n", client_q->sock);
		#endif

		int length = read ( client_q->sock, buffer, RCVBUFFER );
		if (length == 0) {
			/* Invalidate our Socket */
			client_q->destroy = 1;
			close (client_q->sock);
		} 
		if ( ((errno == EAGAIN) && (length < 0)) || length > 0 )
			if ( event_add (&client_q->ev, NULL) == -1 )
				err_handler("event_add() 5 failed");
	}
	if (ev & EV_WRITE) { /* Write the Data the definity sent us */
		struct exten *ex_data = (struct exten *) data;
		char extension[5];

		/* DEBUG */
		#ifdef DEBUG
		fprintf ( stderr, "Entering Client Handler for %d %s", ex_data->sock, (!ex_data->c_client->avaya)?"Asterisk":"Err");
		fprintf ( stderr, "\tExtensions Captured : %d , %d\n", ex_data->incoming, ex_data->outgoing );
		#endif

		sprintf(extension, "%d\n", ex_data->incoming);
		if ( (send (ex_data->sock, extension, strlen(extension), 0)) != strlen(extension) ) {
			ex_data->c_client->destroy = 1;
			close (ex_data->sock);
		} else {
			sprintf(extension, "%d\n", ex_data->outgoing);
			if ( (send (ex_data->sock, extension, strlen(extension), 0)) != strlen(extension) ) {
			   ex_data->c_client->destroy = 1;
			   close (ex_data->sock);
			}
		}
		free (ex_data);
	}
}

void avaya_server (int fd, short ev, void *data) {
	char buffer[RCVBUFFER + 2];
	char response[] = {2,6,1,0,7,1,0,2,1,6,0x81};
	int  message_size = 0,i;
	struct listenq *client_q = (struct listenq *) data; /* Client that triggerred the event */
	struct listenq *c_client;                           /* Current Client for looping thru all */
	bstring str_buff;                                   /* bstring buffer for bufferoverflow and misc. */

	/* DEBUG */
	#ifdef DEBUG
	fprintf ( stderr, "Entering Avaya Handler for %d %s\n", client_q->sock, (client_q->avaya)?"Avaya":"Err");
	#endif

	if (ev & EV_READ) {
		/* Init buffer and bstring buffer */
		buffer[RCVBUFFER] = 0;
		str_buff = bfromcstr ("");
		balloc (str_buff, RCVBUFFER + 4);

		if (( message_size = read(client_q->sock, buffer, RCVBUFFER)) > 0 ) {
			int m;
			/* DEBUG */
			#ifdef DEBUG
			fprintf ( stderr, "\tGetting %d bytes from %d\n", message_size, client_q->sock );
			#endif
	
			if ( message_size == 17 /*&& buffer[0] == 0x01 */) {
				/* 
				 * Send back the Confirmation to the Avaya Client in correspondance
				 * to the Avaya RDT Protocol (Avaya Reliable Data Transport)
				 * Reverse Engineered from RDTT and the help of ethereal
				 */
				if ( send(client_q->sock, response, 11, 0) != 11 )
					err_handler ("send() failed");

				#ifdef DEBUG
				for ( m = 0; m < message_size; m++ )
					fprintf( stderr, "%d ", buffer[m] );
				fprintf ( stderr, "\n" );
				#endif
			} else {
				do {
					/* 
					 * Handle the CDR Data the Definity Sends us as well as special parts of the Protocol
					 *    counter is the special control number the Avaya sends us in the packet
					 *    and which it expects to be sent back + 1
					 */
					counter = ( buffer[0] == -128 && message_size > 5 )? buffer[3] : counter;

					/*
					 * The start of the CDR Data, if the first char is 0x80 then we have 4 bytes 
					 * of RDT specifics.
					 */
					i = ((int)buffer[0] == -128)? 5 : 0;
				
					buffer[message_size] = '\n';
					buffer[message_size + 1] = '\0';
				
					for ( m = i; m < message_size; m++ ) {
						if (buffer[m] == 0)
							i = m + 1;
					}
					/* 
					 * Append the buffer read to our bstring (for easier handling and buffer overflow protection)
					 * without the packet header of RDT if it exists, see `i` above.
					 */
					str_buff = bfromcstr ( &buffer[i] );
					#ifdef DEBUG
					fprintf ( stderr, ".\t\n%s", str_buff->data );
					#endif

					if (message_size == 1) {
						/* RDT Sends periodical 1 byte messages with 0x50 and expects 0x51 sent back */
						if (buffer[0] == 0x50) {
							char respond[] = {0x51};
							if ( send (client_q->sock, respond, 1, 0) != 1)
								err_handler("send() failed");
						}
					} else {
						/* 
						 * RDT Expects the counter sent back after a ' \r\n' at the end of a buffer 
						 * Note :
						 *   This might very well not be the RDT behaviour but it's what we learned
						 *   from ethereal. Since sometimes it was at the end of the buffer, or as
						 *   a new packet with only this to be had.
						 */
						i = message_size - 1;
						if (buffer[i-2] == 0x20 && buffer[i-1] == 0x0d && buffer[i] == 0x0a) {
							char respond[] = {0x40, 0x05, 0x01, ++counter, 0x81};
							if ( send (client_q->sock, respond, 5, 0) != 5 )
								err_handler("send() failed");
						}
					}

					/* Check to see if we have Data to process */
					if (blength(str_buff) > 0) {
						/*
						 * The CDR is received each in a new line, so we process it if we
						 * have newlines left in our buffer.
						 * Here would go code if we wanted to write the CDR to a file or MySQL
						 */
						int nline = bstrchr( str_buff, 10 );
						while ( nline != BSTR_ERR) { 
							if (nline > 64 ) {
								bstring extension1, extension2;
								if (str_buff->data[47] == 32) {
									extension1 = bmidstr (str_buff, 48, 4);
									/* DEBUG */
									#ifdef DEBUG
									fprintf ( stderr, "\t %s\n", extension1->data );
									#endif
									
									btrimws (extension1);
									bcatcstr (extension1, "\n");
								} else
									extension1 = bfromcstr ("");
	
								if (str_buff->data[58] == 32) {
									extension2 = bmidstr (str_buff, 59, 4);
									/* DEBUG */
									#ifdef DEBUG
									fprintf ( stderr, "\t %s\n", extension2->data );
									#endif
									
									btrimws (extension2);
									bcatcstr (extension2, "\n");
								} else
									extension2 = bfromcstr ("");

								c_client = clientq;

								while (c_client != NULL) {
									/* 
									 * Create the structure containing the data we need to handle the write.
									 * This will be freed inside the event handler.
									 */
									if ( (c_client->destroy != 1) && (c_client->avaya != 1) ) {
										/* extension1->data, extension2->data */

										if ( blength(extension1) > 1 )
											if ( (send (c_client->sock, extension1->data, blength(extension1), 0)) != blength(extension1) ) {
												c_client->destroy = 1;
												close (c_client->sock);
											} 
										if ( blength(extension2) > 1 )
											if ( (send (c_client->sock, extension2->data, blength(extension2), 0)) != blength(extension2) ) {
												c_client->destroy = 1;
												close (c_client->sock);
											}
									}	
									c_client = c_client->next;
								}
							}
							/* remove the line we processed */
							if (nline != BSTR_ERR ) {
								bstring new_buff = bmidstr (str_buff, nline + 1, blength(str_buff) - nline );
								if (new_buff != NULL) {
									bdestroy (str_buff);
									str_buff = new_buff;
								} else {
									bdestroy (str_buff);
									nline = BSTR_ERR;
								}
							}
							nline = bstrchr( str_buff, 10 );
						} 
						str_buff = bfromcstr("");
					}
				} while ( (message_size = read (client_q->sock, buffer, RCVBUFFER)) > 0 );
			}
		}
		/* Free up memory allocated */
		bdestroy (str_buff);
	}

	/* Re-schedule the event to read data from the socket */
	if ( ((errno == EAGAIN) && (message_size < 0)) || message_size > 0 )
		if ( event_add (&client_q->ev, NULL) == -1 )
			err_handler("event_add() 5 failed");

	/* Mark the client socket as dead */
	if ( message_size == 0 ) {
		client_q->destroy = 1;
		close(client_q->sock);
	}
}

void err_handler ( char *message ) {
	perror (message);
	exit(1);
}
