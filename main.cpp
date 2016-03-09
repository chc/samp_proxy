#include "main.h"

#include <vector>
#include <iterator>

#include "INetClient.h"
#include "UDPClient.h"

#define PING_TIME 120

std::vector<UDPClient *> clients;

uint32_t resolv(char *host) {
    struct  hostent *hp;
    uint32_t    host_ip;

    host_ip = inet_addr(host);
    if(host_ip == INADDR_NONE) {
        hp = gethostbyname(host);
        if(!hp) {
			return (uint32_t)-1;
        }
        host_ip = *(uint32_t *)hp->h_addr;
    }
    return(host_ip);
}

UDPClient *find_client_by_socket_info(struct sockaddr_in *peer) {
	std::vector<UDPClient *>::iterator iterator=clients.begin();
	UDPClient *user;
	struct sockaddr_in *userpeer;
	while(iterator != clients.end()) {
		user=*iterator;
		userpeer = user->getSockAddr();
		if((userpeer->sin_addr.s_addr == peer->sin_addr.s_addr) && (userpeer->sin_port == peer->sin_port)) {
			return user;
		}
		iterator++;
	}
	return NULL;
}

void deleteClient(UDPClient *client) {
	std::vector<UDPClient *>::iterator iterator;
	iterator=clients.begin();
	while(iterator != clients.end()) {
		if(*iterator==client) {
			iterator = clients.erase(iterator);
			delete client;
		} else {
			iterator++;
		}
	}
}
void check_timeouts() {
	std::vector<UDPClient *>::iterator iterator=clients.begin();
	UDPClient *user;
	while(iterator != clients.end()) {
		user=*iterator;
		if(time(NULL)-PING_TIME > user->getLastRecvTime()) {
			deleteClient(user);
			iterator = clients.begin();
			continue;
		}
		iterator++;
	}
}
int main() {
	#ifdef _WIN32
    WSADATA wsdata;
    WSAStartup(MAKEWORD(1,0),&wsdata);
	#endif
	//char *server_dest_ip = "samp.nl-rp.net";
	char *server_dest_ip = "samp.nl-rp.net";
	uint16_t server_dest_port = 7777;

	//char *bind_ip = "192.168.10.67";
	char *bind_ip = "0.0.0.0";
	uint16_t server_source_port = 7777;

	uint32_t server_ip = resolv(server_dest_ip);
	//uint32_t server_bind_ip = resolv(bind_ip);
	uint32_t server_bind_ip = INADDR_ANY;

	int server_sd;
	if((server_sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		printf("socket error\n");
		//signal error
    }

	struct sockaddr_in local_addr;
    local_addr.sin_port = htons(server_source_port);
    local_addr.sin_addr.s_addr = htonl(server_bind_ip);
    local_addr.sin_family = AF_INET;



	int n = bind(server_sd, (struct sockaddr *)&local_addr, sizeof local_addr);
    if(n < 0) {
    	printf("bind error\n");
        //signal error
    }

    int hsock = server_sd+1;
	fd_set  fdset;
	struct timeval timeout;
	printf("Enter loop\n");

	UDPClient *client = NULL;


    while(true) {
		

		memset(&timeout,0,sizeof(struct timeval));

		timeout.tv_usec = 16000;

		FD_ZERO(&fdset);
		FD_SET(server_sd, &fdset);

		for(std::vector<UDPClient *>::iterator it = clients.begin(); it != clients.end();it++) {
			UDPClient *c = *it;
			FD_SET(c->getServerSocket(), &fdset);
		}
		if(select(hsock, &fdset, NULL, NULL, &timeout) < 0) {
			check_timeouts();
			continue;
		}

		if(FD_ISSET(server_sd, &fdset)) {
			char recvbuf[1024];
		    struct sockaddr_in si_other;
		    socklen_t slen = sizeof(struct sockaddr_in);

			int len = recvfrom(server_sd,(char *)&recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&si_other,&slen);
			UDPClient *c = find_client_by_socket_info(&si_other);
			if(c) {
				client = c;
			} else {
				client = new UDPClient(server_sd, &si_other, server_ip, server_dest_port, server_source_port);
				clients.push_back(client);
			}
			if(client->getServerSocket()+1 > hsock) {
				hsock = client->getServerSocket()+1;
			}
			client->process_packet((char *)&recvbuf, len);
		}
		for(std::vector<UDPClient *>::iterator it = clients.begin(); it != clients.end();it++) {
			UDPClient *c = *it;
			if(FD_ISSET(c->getServerSocket(), &fdset)) {
				c->readServer();
			}
		}
		check_timeouts();
	}
}
