#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

int		clients = 0, fd_max = 0;
int		id[10000];
char	*msg[10000];
char	rbuf[1024], wbuf[42];
fd_set	rfds, wfds, fds;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	notify(int from, char *s)
{
	for (int fd = 0; fd <= fd_max; fd++)
		if (FD_ISSET(fd, &wfds) && fd != from)
			send(fd, s, strlen(s), 0);
}

void	serve(int fd)
{
	char	*s;
	while (extract_message(&(msg[fd]), &s))
	{
		sprintf(wbuf, "client %d: ", id[fd]);
		notify(fd, wbuf);
		notify(fd, s);
		free(s);
		s = NULL;
	}
}

void	add_client(int fd)
{
	fd_max = fd > fd_max ? fd : fd_max;
	id[fd] = clients++;
	msg[fd] = NULL;
	FD_SET(fd, &fds);
	sprintf(wbuf, "server: client %d just arrived\n", id[fd]);
	notify(fd, wbuf);
}

void	remove_client(int fd)
{
	sprintf(wbuf, "server: client %d just left\n", id[fd]);
	notify(fd, wbuf);
	free(msg[fd]);
	msg[fd] = NULL;
	FD_CLR(fd, &fds);
	close(fd);
}

int		create_socket(void)
{
	fd_max = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_max < 0)
		fatal_error();
	FD_SET(fd_max, &fds);
	return (fd_max);
}

int		main(int ac, char **av)
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	FD_ZERO(&fds);
	int	sockfd = create_socket();
	struct sockaddr_in	servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));
	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) || listen(sockfd, SOMAXCONN))
		fatal_error();
	while (1)
	{
		rfds = wfds = fds;
		if (select(fd_max + 1, &rfds, &wfds, NULL, NULL) < 0)
			fatal_error();
		for (int fd = 0; fd <= fd_max; fd++)
		{
			if (!FD_ISSET(fd, &rfds))
				continue;
			if (fd == sockfd)
			{
				socklen_t	len = sizeof(servaddr);
				int			client = accept(sockfd, (struct sockaddr *)&servaddr, &len);
				if (client >= 0)
				{
					add_client(client);
					break;
				}
			}
			else
			{
				int	r = recv(fd, rbuf, 1024, 0);
				if (r <= 0)
				{
					remove_client(fd);
					break;
				}
				rbuf[r] = '\0';
				msg[fd] = str_join(msg[fd], rbuf);
				serve(fd);
			}
		}
	}
	return (0);
}