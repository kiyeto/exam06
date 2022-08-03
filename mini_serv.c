#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>	// for test

typedef struct		s_cli
{
	int				id;
	int				fd;
	char			*buf;
	struct s_cli	*next;
}					t_cli;

t_cli	*clients = 0;
fd_set	master, reads, writes;
int		id = 0;

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

void fatal_err(int x)
{
	if (!x)
		write(2, "Fatal error\n", strlen("Fatal error\n"));	
	else
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));	
	exit(1);
}

void add_cli(int fd, int id)
{
	t_cli *copy, *new;
	copy = clients;
	if (!(new = calloc(1, sizeof(t_cli))))
		fatal_err(0);
	new->id = id;
	new->fd = fd;
	new->buf = 0;
	new->next = 0;
	if (!clients)
		clients = new;
	else
	{
		while (copy->next)
			copy = copy->next;
		copy->next = new;
	}
}

void rm_cli(t_cli **c)
{
	t_cli *before;
	
	if (clients == *c)
		clients = clients->next;
	else
	{
		before = clients;
		while (before->next != *c)
			before = before->next;
		before->next = (*c)->next;
	}
	FD_CLR((*c)->fd, &master);
	close((*c)->fd);
	free((*c)->buf);
	free(*c);
}

void broadcast(char *msg, int id)
{
	t_cli *copy = clients;
	while (copy)
	{
		if (FD_ISSET(copy->fd, &writes) && copy->id != id)
			send(copy->fd, msg, strlen(msg), 0);
		copy = copy->next;
	}
}

int main(int ac, char **av)
{
	if (ac != 2)
		fatal_err(1);
	int servfd, clifd, port, maxfd;
	char buffer[1024], defined[100];
	struct sockaddr_in servaddr; 
	port = atoi(av[1]);
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
	if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 || (bind(servfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) < 0 || listen(servfd, 10) < 0)
		fatal_err(0);
	FD_ZERO(&master);
	FD_SET(servfd, &master);
	maxfd = servfd;
	while (1)
	{
		reads = writes = master;
		if (select(maxfd + 1, &reads, &writes, NULL, NULL) < 0)
			continue;
		if (FD_ISSET(servfd, &reads))
		{
			clifd = accept(servfd, NULL, NULL);
			fcntl(clifd, F_SETFL, O_NONBLOCK);	// for test
			FD_SET(clifd, &master);
			maxfd = (maxfd > clifd) ? maxfd : clifd;
			sprintf(defined, "server: client %d just arrived\n", id);
			add_cli(clifd, id);
			broadcast(defined, id++);
		}
		t_cli *copy = clients;
		while (copy)
		{
			if (FD_ISSET(copy->fd, &reads))
			{
				int ret;
				while ((ret = recv(copy->fd, buffer, 1023, 0)) > 0)
				{
					buffer[ret] = 0;
					copy->buf = str_join(copy->buf, buffer);
				}
				if (ret == 0)
				{
					sprintf(defined, "server: client %d just left\n", copy->id);
					broadcast(defined, copy->id);
					rm_cli(&copy);
					break;
				}
				char *msg = 0;
				while (extract_message(&copy->buf, &msg) == 1)
				{
					char *line = 0;
					if (!(line = malloc(sizeof(char) * strlen(msg) + 15)))
						fatal_err(1);
					sprintf(line, "client %d: %s", copy->id, msg);
					broadcast(line, copy->id);
					free(line);
					free(msg);
				}
			}
			copy = copy->next;
		}
	}
}