#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <fcntl.h>


typedef struct		s_cli
{
	int				id;
	int				fd;
	char 			*buff;
	struct s_cli	*next;
}					t_cli;

t_cli *g_list = NULL;
fd_set master, reads, writes;

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

void fatal_err(int n)
{
	if (!n)
		write(2, "Fatal error\n", 12);
	else
		write(2, "Wrong number of arguments\n", 26);
	exit(1);
}

void add_cli(int fd, int *id)
{
	t_cli *list = g_list;
	t_cli *new;

	if (!(new = calloc(1, sizeof(t_cli))))
		fatal_err(0);
	
	new->id = (*id)++;
	new->fd = fd;
	new->buff = 0;
	if (!g_list)
		g_list = new;
	else
	{
		while (list->next)
			list = list->next;
		list->next = new;
	}
}

void rm_cli(t_cli **c)
{
	t_cli *before = NULL;	
	
	if (g_list == *c)
		g_list = (*c)->next;
	else
	{
		before = g_list;
		while (before->next != *c)
			before = before->next;
		before->next = (*c)->next;
	}
	FD_CLR((*c)->fd, &master);
	close((*c)->fd);
	free((*c)->buff);
	free(*c);
}

t_cli *find_cli(int i)
{
	t_cli *list_c, *c;
	list_c = g_list;
	while (list_c)
	{
		if (list_c->fd == i)
		{
			c = list_c;
			break;
		}
		list_c = list_c->next;
	}
	return c;
}

void broadcast(int id, char *msg)
{
	t_cli *list = g_list;

	while (list && FD_ISSET(list->fd, &writes))
	{
		if (list->id != id)
		{
			if (send(list->fd, msg , strlen(msg), 0) < 0)
				fatal_err(0);
		}
		list = list->next;
	}
}

int main(int ac, char **av)
{
	if (ac!= 2)
		fatal_err(1);
	int serverfd, clientfd;
	struct sockaddr_in servaddr;
	size_t port;
	int id = 0;
	char defined[100];

	bzero(&servaddr, sizeof(servaddr));
	port = atoi(av[1]);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);
	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 || bind(serverfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 || listen(serverfd, 10) < 0 )
		fatal_err(0);
	FD_ZERO(&master);
	FD_SET(serverfd , &master);
	int max = serverfd;
	char buffer[1024];
	while (1)
	{
		reads = writes = master;
		if (select(FD_SETSIZE, &reads, &writes, NULL, NULL) < 0)
			continue ;
		
		for (int i = 0; i <= max; i++)
		{
			if (FD_ISSET(i , &reads))
			{
				if (i == serverfd)
				{
					if ((clientfd = accept(serverfd, NULL, NULL)) < 0)
						fatal_err(0);
					fcntl(clientfd, F_SETFL, O_NONBLOCK);
					FD_SET(clientfd, &master);
					max = clientfd > max ? clientfd : max;
					bzero(&defined, sizeof(defined));
					sprintf(defined, "server: client %d just arrived\n", id);
					add_cli(clientfd, &id);
					broadcast(id, defined);
					break;
				}
				else
				{
					t_cli *c = find_cli(i);
					int	ret;
					while ((ret = recv(i, &buffer, 1023, 0)) > 0)
					{
						buffer[ret] = 0;
						c->buff = str_join(c->buff, buffer);
					}
					if (ret == 0)
					{
						sprintf(defined, "server: client %d just left\n", c->id);
						broadcast(c->id, defined);
						rm_cli(&c);
						break;
					}
					char *msg = 0;
					while ((ret = extract_message(&c->buff, &msg)) == 1)
					{
						char *line = 0;
						if (!(line = malloc(sizeof(char) * (strlen(msg) + 15))))
							fatal_err(0);
						sprintf(line, "client %d: %s", c->id, msg);
						broadcast(c->id, line);
						free(msg);
						free(line);
					}
					if (ret < 0)
						fatal_err(0);
				}
			}
		}
		system("leaks a.out");
	}
}