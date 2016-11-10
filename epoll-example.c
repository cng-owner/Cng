#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAXEVENTS 64

static int
make_socket_non_blocking (int socket_fd)
{
	int flags, s;

	flags = fcntl (socket_fd, F_GETFL, 0);
	if (flags == -1)
	{
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl (socket_fd, F_SETFL, flags);
	if (s == -1)
	{
		perror ("fcntl");
		return -1;
	}

	return 0;
}

// Setup a socket listening on the specified local port
static int
create_and_bind (char *port)
{
	// Specify default socket characteristics for getaddrinfo()
	struct addrinfo hints;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE;     /* All interfaces */

	// Fetch socket address identifications... allows for IPV4 & IPV6.
	struct addrinfo *ai_list;
	int gai_result = getaddrinfo (NULL, port, &hints, &ai_list);
	if (gai_result != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (gai_result));
		return -1;
	}

	// How can a specific device be specified so that ETH1 and ETH0 can be
	// associated with different sockets?
	int socket_fd = -1;

	struct addrinfo *ai_ptr;
	for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next)
	{
		// Create the socket and get its handle
		// ? Can't O_NONBLOCK be specified here ?
		socket_fd = socket(
			ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol );
		if( -1 == socket_fd )
			continue;

		// Connect each socket to a port
		int bind_result = bind (socket_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
		if (bind_result == 0)
		{
			/* We managed to bind successfully! */

			// This would probably have to be different if IPV6 and IPV4 were
			// to be supported at the same time, but for now use first available
			break;
		}

		close (socket_fd);
	}

	freeaddrinfo (ai_list);
	if (ai_ptr == NULL)
	{
		fprintf (stderr, "Could not bind\n");
		return -1;
	}


	return socket_fd;
}

int
main (int argc, char *argv[])
{
	int socket_fd, s;
	int epoll_fd;
	struct epoll_event event;
	struct epoll_event *events;

	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	socket_fd = create_and_bind (argv[1]);
	if (socket_fd == -1)
		abort ();

	// This should be done when creating the socket
	s = make_socket_non_blocking (socket_fd);
	if (s == -1)
		abort ();

	// Let the OS know to listen for incoming connections
	// SOMAXCONN is the maximum number of pending connections...
	// it seems as if 128 is pretty high.
	s = listen (socket_fd, SOMAXCONN);
	if (s == -1)
	{
		perror ("listen");
		abort ();
	}

	epoll_fd = epoll_create1 (0);
	if (epoll_fd == -1)
	{
		perror ("epoll_create");
		abort ();
	}

	event.data.fd = socket_fd;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl (epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
	if (s == -1)
	{
		perror ("epoll_ctl");
		abort ();
	}

	/* Buffer where events are returned */
	events = calloc (MAXEVENTS, sizeof event);

	/* The event loop */
	for (;;)
	{
		int n, i;

		n = epoll_wait (epoll_fd, events, MAXEVENTS, -1);
		for (i = 0; i < n; i++)
		{
			if ((events[i].events & EPOLLERR)
			||  (events[i].events & EPOLLHUP)
			||  (!(events[i].events & EPOLLIN)))
			{
				/* An error has occured on this fd, or the socket is not
				   ready for reading (why were we notified then?) */
				fprintf (stderr, "epoll error\n");
				close (events[i].data.fd);
				continue;
			}

			else if (socket_fd == events[i].data.fd)
			{
				/* We have a notification on the listening socket, which
				   means one or more incoming connections. */
				while (1)
				{
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

					in_len = sizeof in_addr;
					infd = accept (socket_fd, &in_addr, &in_len);
					if (infd == -1)
					{
						if( (errno == EAGAIN)
						|| (errno == EWOULDBLOCK))
						{
							/* We have processed all incoming
							   connections. */
							break;
						}
						else
						{
							perror ("accept");
							break;
						}
					}

					s = getnameinfo (&in_addr, in_len,
							hbuf, sizeof hbuf,
							sbuf, sizeof sbuf,
							NI_NUMERICHOST | NI_NUMERICSERV);
					if (s == 0)
					{
						printf("Accepted connection on descriptor %d "
								"(host=%s, port=%s)\n", infd, hbuf, sbuf);
					}

					/* Make the incoming socket non-blocking and add it to the
					   list of fds to monitor. */
					s = make_socket_non_blocking (infd);
					if (s == -1)
						abort ();

					event.data.fd = infd;
					event.events = EPOLLIN | EPOLLET;
					s = epoll_ctl (epoll_fd, EPOLL_CTL_ADD, infd, &event);
					if (s == -1)
					{
						perror ("epoll_ctl");
						abort ();
					}
				}
				continue;
			}
			else
			{
				/* We have data on the fd waiting to be read. Read and
				   display it. We must read whatever data is available
				   completely, as we are running in edge-triggered mode
				   and won't get a notification again for the same
				   data. */
				int done = 0;

				while (1)
				{
					ssize_t count;
					char buf[512];

					count = read (events[i].data.fd, buf, sizeof buf);
					if (count == -1)
					{
						/* If errno == EAGAIN, that means we have read all
						   data. So go back to the main loop. */
						if (errno != EAGAIN)
						{
							perror ("read");
							done = 1;
						}
						break;
					}
					else if (count == 0)
					{
						/* End of file. The remote has closed the
						   connection. */
						done = 1;
						break;
					}

					/* Write the buffer to standard output */
					s = write (1, buf, count);
					if (s == -1)
					{
						perror ("write");
						abort ();
					}
				}

				if (done)
				{
					printf ("Closed connection on descriptor %d\n",
							events[i].data.fd);

					/* Closing the descriptor will make epoll remove it
					   from the set of descriptors which are monitored. */
					close (events[i].data.fd);
				}
			}
		}
	}

	free (events);

	close (socket_fd);

	return EXIT_SUCCESS;
}
