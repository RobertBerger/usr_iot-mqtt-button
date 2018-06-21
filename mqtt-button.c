/*
 * stdin publisher
 *
 * compulsory parameters:
 *
 * --topic topic to publish on
 *
 * defaulted parameters:
 *
 *  --host :: localhost
 *  --port :: 1883
 *  --qos :: 0
 *  --delimiters :: \n
 *  --clientid :: stdin-publisher-async
 *  --maxdatalen :: 100
 *  --keepalive :: 10
 *
 *  --userid :: none
 *  --password :: none
 *
 * gcc -I/usr/local/include -L/usr/local/lib/ -o select_mqtt_async_pub select_mqtt_async_pub.c -lpaho-mqtt3a
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/select.h>

#include <MQTTAsync.h>

#define DEVICE_PATH "/dev/input/event2"

struct {
	char *clientid;
	char *delimiter;
	int maxdatalen;
	int qos;
	int retained;
	char *username;
	char *password;
	char *host;
	char *port;
	int verbose;
	int keepalive;
} opts = {
"stdin-publisher-async", "\n", 100, 0, 0, NULL, NULL, "localhost",
	    "1883", 0, 10};

void getopts(int argc, char **argv);
void myconnect(MQTTAsync * client);

void usage(void)
{
	printf("MQTT stdin publisher\n");
	printf("Usage: stdinpub topicname <options>, where options are:\n");
	printf("    --host <hostname> (default is %s)\n", opts.host);
	printf("    --port <port> (default is %s)\n", opts.port);
	printf("    --qos <qos> (default is %d)\n", opts.qos);
	printf("    --retained (default is %s)\n",
	       opts.retained ? "on" : "off");
	printf("    --delimiter (default is \\n)\n");
	printf("    --clientid <clientid> (defaults is %s)\n", opts.clientid);
	printf("    --maxdatalen <bytes> (default is %d)\n", opts.maxdatalen);
	printf("    --username none\n");
	printf("    --password none\n");
	printf("    --keepalive <seconds> (default is 10 seconds)\n");

	exit(EXIT_FAILURE);
}

volatile int toStop = 0;	/* why is this volatile and not static ? */

/* this is a signal handler */
void cfinish(int sig)
{
	sig = sig;
	/* you want to send a signal
	   from a signal handler
	   which the handler is subscribed to
	   endless loop? */
	signal(SIGINT, NULL);	/* what is this? */

	toStop = 1;		/* this is not necessarily atomic */
}

static int connected = 0;	/* that's a shared resource */
static int disconnected = 0;	/* that's a shared resource */

void onConnectFailure(void *context, MQTTAsync_failureData * response)
{
	MQTTAsync client;
	printf("Connect failed, rc is %d\n", response ? response->code : -1);	/* don't use ? - unreadable code */
	connected = -1;		/* not necessarily atomic */

	client = (MQTTAsync) context;
	myconnect(client);
}

void onConnect(void *context, MQTTAsync_successData * response)
{
	context = context;
	response = response;
	printf("Connected");
	connected = 1;		/* not necessarily atomic */
}

void connectionLost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync) context;
	MQTTAsync_connectOptions conn_opts =
	    MQTTAsync_connectOptions_initializer;
	MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
	int rc = 0;
	ssl_opts = ssl_opts;

	cause = cause;

	printf("Connecting\n");
	conn_opts.keepAliveInterval = 10;
	conn_opts.cleansession = 1;
	conn_opts.username = opts.username;
	conn_opts.password = opts.password;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;

	ssl_opts.enableServerCertAuth = 0;
	/*conn_opts.ssl_opts = &ssl_opts; */

	connected = 0;

	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
		printf("Failed to start connect, return code is %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

void myconnect(MQTTAsync * client)
{
	MQTTAsync_connectOptions conn_opts =
	    MQTTAsync_connectOptions_initializer;
	MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
	int rc;
	ssl_opts = ssl_opts;
	rc = 0;

	printf("Connecting\n");
	conn_opts.keepAliveInterval = opts.keepalive;
	conn_opts.cleansession = 1;
	conn_opts.username = opts.username;
	conn_opts.password = opts.password;

	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;

	conn_opts.context = client;

	ssl_opts.enableServerCertAuth = 0;
	conn_opts.automaticReconnect = 1;

	connected = 0;

	if ((rc = MQTTAsync_connect(*client, &conn_opts)) != MQTTASYNC_SUCCESS) {
		printf("Failed to connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

int messageArrived(void *context, char *topicName, int topicLen,
		   MQTTAsync_message * m)
{
	context = context;
	topicName = topicName;
	topicLen = topicLen;
	m = m;
	/* Not expecting any messages */
	return 1;
}

void onDisconnect(void *context, MQTTAsync_successData * response)
{
	context = context;
	response = response;
	disconnected = 1;
}

static int published = 0;

void onPublish(void *context, MQTTAsync_successData * response)
{
	context = context;
	response = response;
	published = 1;
}

void onPublishFailure(void *context, MQTTAsync_failureData * response)
{
	context = context;
	response = response;
	printf("Published failed, return code is %d\n",
	       response ? response->code : -1);
	published = -1;
}

/*
 * Main()
 */
int main(int argc, char **argv)
{
	char url[100];
	char *topic = NULL;
	int rc = 0;
	MQTTAsync client;
	char *buffer = NULL;

	char buffer2[] = "Button Pressed\n";
	int delim_len;

	/* select() system call configuration arguments
	 * ******************************************** */
	struct timeval *pto;

	/* select() ... returns number of ready file descriptors
	 *  0    on timeout
	 * -1   on failure
	 */
	int ready;

	/* File descriptor sets
	 * readfsd      .. set of file descriptors / if INPUT is possible
	 * writefds     .. set of file descriptors / if OUTPUT is possible
	 * exceptfds    .. set of file descriptors / if EXCEPTIONAL condition occurred
	 */
	fd_set readfds;

	/* nfds .. should be set to the highest-number file descriptor
	 *         in any of the three-sets plus(+) 1
	 */
	int nfds;

	/* An abstract indicator used to access an input/output resource */
	int fd;

	/* Is there a specific reason those are not global variables ? */
	/* as global vars we could move MQTT malas in specific functions */

	MQTTAsync_createOptions create_opts =
	    MQTTAsync_createOptions_initializer;
	MQTTAsync_responseOptions pub_opts =
	    MQTTAsync_responseOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts =
	    MQTTAsync_disconnectOptions_initializer;

	if (argc < 2) {
		usage();
	}

	getopts(argc, argv);

	sprintf(url, "%s:%s", opts.host, opts.port);

	if (opts.verbose) {
		printf("Broker URL is %s\n", url);
	}

	topic = argv[1];
	printf("Using Topic %s\n", topic);

	/* also this could go into some different function out of main */

	create_opts.sendWhileDisconnected = 1;
	rc = MQTTAsync_createWithOptions(&client, url, opts.clientid,
					 MQTTCLIENT_PERSISTENCE_NONE, NULL,
					 &create_opts);

	/* what should be happening with rc?
	 * Error handling?
	 * since we are no in the loop here, maybe loop and retry?
	 * (performance) Variable 'rc' is reassigned a value before the old one has been used.
	 */

	/* I guess here you want to connect SIGINT
	 * and SIGTERM with the cfinish signal handler
	 */

  /* besides "signal" is old - use sigaction instead */

	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

	rc = MQTTAsync_setCallbacks(client, client, connectionLost,
				    messageArrived, NULL);

	/* what should be happening with rc?
	 * Error handling?
	 * since we are no in the loop here, maybe loop and retry?
	 * (performance) Variable 'rc' is reassigned a value before the old one has been used.
	 */

	myconnect(&client);

	/* after connect MQTT is active?
	 * I guess like this the malloc should be before connect
	 */

	/* where does opts.maxdatalen come from?
	 * commandline ?
	 */

	buffer = malloc(opts.maxdatalen);

	/* *** SELECT() system call configuration *** */
	/* ****************************************** */

	/* Timeout for select() */
	pto = NULL;		/* Infinite timeout */

	/* Highest numbered file descriptor */
	nfds = 0;
	FD_ZERO(&readfds);

	/* Open Device File */
	if ((fd = open(DEVICE_PATH, O_RDONLY)) < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	FD_SET(fd, &readfds);

	if (fd > nfds) {
		nfds = fd + 1;
	}

	while (!toStop) { /* !!!!!!!!!!! here our event loop starts !!!!!!!!!!!!!! */
		int data_len = 0;

		/*
		 * I/O Multiplexing ..
		 * Simultaneous monitor many file descriptors and see if
		 * read/ write is possible on any of them.
		 */

		/* ************ End of select() system call **********
		 * ******* to monitor multiple file descriptors ******
		 */

		/* here should be pselect instead of select */
		/* something like:
		   sigaddset(&ss, SIGWHATEVER);
		   ready = pselect(nfds, &readfds, NULL, NULL, pto, &ss);
		 */
		ready = select(nfds, &readfds, NULL, NULL, pto);

		if (ready == -1) {
			/* An error occured */
			perror("select");
			exit(EXIT_FAILURE);
		}
		/* else ?*/
		if (ready == 0) {
			/* Call was timed out */
			;
		}


		/*else if (ready == 1) {
		   // The device file descriptor block released
		   } */

		/* Ready for Select() Wakeup ... Device File is changed */
		/* char buffer2[] = "Button Pressed\n"; - moved further up */

		/* Read message-to-send from terminal */
		/* int delim_len = 0; *//* why???? */

		delim_len = (int)strlen(opts.delimiter);

		do {
			buffer[data_len] = buffer2[data_len];
			data_len++;

			if (data_len > delim_len) {
				if (strncmp
				    (opts.delimiter,
				     &buffer[data_len - delim_len],
				     delim_len) == 0) {
					break;
				}
			}
		} while (data_len < opts.maxdatalen);

		if (opts.verbose) {
			printf("Publishing data of length %d\n", data_len);
		}

    /* outside of the loop? */
		pub_opts.onSuccess = onPublish;
		pub_opts.onFailure = onPublishFailure;

    /* endless loop for sending? */
		/* add a counter here how often we loop */
		do {
			rc = MQTTAsync_send(client, topic, data_len, buffer,
					    opts.qos, opts.retained, &pub_opts);
		} while (rc != MQTTASYNC_SUCCESS);
	} /* !!!!!!!!!!! here our event loop ends !!!!!!!!!!!!!! */

	printf("Stopping\n");

	free(buffer);

  /* maybe all this below should go into another function */

	disc_opts.onSuccess = onDisconnect;

  /* does not seem to clean up ?, just exit */
	if ((rc =
	     MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS) {
		printf("Failed to start disconnect, return code is %d\n", rc);
		exit(EXIT_FAILURE);
	}

  /* what will this help ? */
	/* when we are not disconnected sleep ????? why ????*/
	/* but no prob, since we are on the error path here */
	while (!disconnected) {
		/* usleep(10000L); *//* POSIX.1-2008 removes the specification of usleep(). */
		sleep(1);
	}

	MQTTAsync_destroy(&client);

	return EXIT_SUCCESS;
}

/*
 * End of Main()
 */

/* should be replaced by getopt lib */

void getopts(int argc, char **argv)
{
	int count = 2;

	while (count < argc) {
		if (strcmp(argv[count], "--retained") == 0) {
			opts.retained = 1;
		}

		if (strcmp(argv[count], "--verbose") == 0) {
			opts.verbose = 1;
		}

		else if (strcmp(argv[count], "--qos") == 0) {
			if (++count < argc) {
				if (strcmp(argv[count], "0") == 0) {
					opts.qos = 0;
				} else if (strcmp(argv[count], "1") == 0) {
					opts.qos = 1;
				} else if (strcmp(argv[count], "2") == 0) {
					opts.qos = 2;
				} else {
					usage();
				}
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--host") == 0) {
			if (++count < argc) {
				opts.host = argv[count];
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--port") == 0) {
			if (++count < argc) {
				opts.port = argv[count];
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--clientid") == 0) {
			if (++count < argc) {
				opts.clientid = argv[count];
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--username") == 0) {
			if (++count < argc) {
				opts.username = argv[count];
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--password") == 0) {
			if (++count < argc) {
				opts.password = argv[count];
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--maxdatalen") == 0) {
			if (++count < argc) {
				opts.maxdatalen = atoi(argv[count]);
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--delimiter") == 0) {
			if (++count < argc) {
				opts.delimiter = argv[count];
			} else {
				usage();
			}
		} else if (strcmp(argv[count], "--keepalive") == 0) {
			if (++count < argc) {
				opts.keepalive = atoi(argv[count]);
			} else {
				usage();
			}
		}

		count++;

	}
}
