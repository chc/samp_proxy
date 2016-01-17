#include "INetClient.h"
#include "UDPClient.h"
#include "encryption.h"
#include <stdio.h>
#include <memory.h>
#define SAMP_MAGIC 0x504d4153

typedef struct {
	uint32_t magic;
	uint32_t ip;
	uint16_t port;
	uint8_t opcode;
} SAMPHeader;

enum PacketEnumeration
{
	ID_INTERNAL_PING = 6,
	ID_PING,
	ID_PING_OPEN_CONNECTIONS,
	ID_CONNECTED_PONG,
	ID_REQUEST_STATIC_DATA,
	ID_CONNECTION_REQUEST,
	ID_AUTH_KEY,
	ID_BROADCAST_PINGS = 14,
	ID_SECURED_CONNECTION_RESPONSE,
	ID_SECURED_CONNECTION_CONFIRMATION,
	ID_RPC_MAPPING,
	ID_SET_RANDOM_NUMBER_SEED = 19,
	ID_RPC,
	ID_RPC_REPLY,
	ID_DETECT_LOST_CONNECTIONS = 23,
	ID_OPEN_CONNECTION_REQUEST,
	ID_OPEN_CONNECTION_REPLY,
	ID_OPEN_CONNECTION_COOKIE,
	ID_RSA_PUBLIC_KEY_MISMATCH = 28,
	ID_CONNECTION_ATTEMPT_FAILED,
	ID_NEW_INCOMING_CONNECTION = 30,
	ID_NO_FREE_INCOMING_CONNECTIONS = 31,
	ID_DISCONNECTION_NOTIFICATION,	
	ID_CONNECTION_LOST,
	ID_CONNECTION_REQUEST_ACCEPTED,
	ID_CONNECTION_BANNED = 36,
	ID_INVALID_PASSWORD,
	ID_MODIFIED_PACKET,
	ID_PONG,
	ID_TIMESTAMP,
	ID_RECEIVED_STATIC_DATA,
	ID_REMOTE_DISCONNECTION_NOTIFICATION,
	ID_REMOTE_CONNECTION_LOST,
	ID_REMOTE_NEW_INCOMING_CONNECTION,
	ID_REMOTE_EXISTING_CONNECTION,
	ID_REMOTE_STATIC_DATA,
	ID_ADVERTISE_SYSTEM = 55,

	ID_PLAYER_SYNC = 207,
	ID_MARKERS_SYNC = 208,
	ID_UNOCCUPIED_SYNC = 209,
	ID_TRAILER_SYNC = 210,
	ID_PASSENGER_SYNC = 211,
	ID_SPECTATOR_SYNC = 212,
	ID_AIM_SYNC = 203,
	ID_VEHICLE_SYNC = 200,
	ID_RCON_COMMAND = 201,
	ID_RCON_RESPONCE = 202,
	ID_WEAPONS_UPDATE = 204,
	ID_STATS_UPDATE = 205,
	ID_BULLET_SYNC = 206,
};

UDPClient::UDPClient(int sd, struct sockaddr_in *si_other, uint32_t server_ip, uint16_t server_port) {
	m_sd = sd;
	memcpy(&m_address_info, si_other, sizeof(m_address_info));

	m_server_addr.sin_family=AF_INET;  
    m_server_addr.sin_addr.s_addr=(server_ip);  
    m_server_addr.sin_port=htons(server_port);  

    m_server_socket = socket(AF_INET, SOCK_DGRAM, 0);

    m_server_port = server_port;
    m_server_ip = server_ip;

    connect(m_server_socket, (struct sockaddr *)&m_server_addr, sizeof(m_server_addr));

    printf("Server IP: %s\n",inet_ntoa(m_server_addr.sin_addr));
}
UDPClient::~UDPClient() {

}
void UDPClient::process_packet(char *buff, int len) {
	SAMPHeader *header = (SAMPHeader *)buff;
	if(header->magic != SAMP_MAGIC) {
		process_game_packet(buff, len, true);
		return;
	}
	printf("Opcode: %c\n",header->opcode);

	m_proxy_addr.sin_port = header->port;
	m_proxy_addr.sin_addr.s_addr = header->ip;

	header->port = htons(m_server_port);
	header->ip = htonl(m_server_ip);

	socklen_t slen = sizeof(struct sockaddr_in);
	sendto(m_server_socket,(char *)buff,len,0,(struct sockaddr *)&m_server_addr, slen);

	struct sockaddr_in temp;
	temp.sin_addr.s_addr = header->ip;
	//printf("SAMP IP: %s %d\n",inet_ntoa(m_server_addr.sin_addr), htons(header->port));
}
int UDPClient::getServerSocket() {
	return m_server_socket;
}
void UDPClient::readServer() {
	char recvbuf[1024];
	socklen_t slen = sizeof(struct sockaddr_in);
	int len = recvfrom(m_server_socket,(char *)&recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&m_server_addr,&slen);
	printf("Got server resp\n");
	SAMPHeader *header = (SAMPHeader *)&recvbuf;
	if(header->magic != SAMP_MAGIC) {
		process_game_packet((char *)&recvbuf, len, false);
		return;
	}
	header->port = m_proxy_addr.sin_port;
	header->ip = m_proxy_addr.sin_addr.s_addr;

	slen = sizeof(struct sockaddr_in);

	struct sockaddr_in temp;
	temp.sin_addr.s_addr = header->ip;
//	printf("SAMP send IP: %s %d\n",inet_ntoa(temp.sin_addr), htons(header->port));

	sendto(m_sd, (char *)&recvbuf, len, 0, (struct sockaddr *)&m_address_info, slen);
//	printf("got resp opcode: %c\n",header->opcode);
}

void UDPClient::process_game_packet(char *buff, int len, bool client_to_server) {
	/*
	void sampEncrypt(uint8_t *buf, int len, int port, int unk);
void sampDecrypt(uint8_t *buf, int len, int port, int unk);
	*/
	static FILE *fd = NULL;
	if(!fd) {
		fd = fopen("dump.bin", "wb");
	}
	socklen_t slen = sizeof(struct sockaddr_in);
	if(client_to_server) {
		//sampDecrypt((uint8_t*)buff, len, htons(m_proxy_addr.sin_port), 0);

		//
		sampDecrypt((uint8_t*)buff, len, (m_server_port), 0);
		switch(buff[0]) {
			case ID_OPEN_CONNECTION_REQUEST:
				printf("[C->S] Connect request packet\n");
			break;
			case ID_AUTH_KEY:
			{
				printf("[C->S] Auth Key\n");
				break;
			}
			default:
			printf("[C->S] Got unknown packet ID: %lu %02X\n",buff[0],buff[0]);
			return;
			break;
		}
		//printf("client sent Packet ID: %02X/%d\n",buff[0],buff[0]);
		fwrite(buff, len, 1, fd);

		for(int i=0;i<4;i++) {
			char c = 0xFF;
			fwrite(&c, 1, 1, fd);
			fflush(fd);
		}
		
		fflush(fd);
		sampEncrypt((uint8_t*)buff, len-1, (m_server_port), 0);		
		sendto(m_server_socket,(char *)buff,len,0,(struct sockaddr *)&m_server_addr, slen);



		printf("Sending %d packet to server\n", len);
	} else {
		switch(buff[0]) {
			case ID_OPEN_CONNECTION_COOKIE: {
				printf("[S->C] Cookie Request\n");
				break;
			}
			case ID_OPEN_CONNECTION_REPLY:
			printf("[S->C] Open Connection Reply\n");
			break;
			case ID_AUTH_KEY:
			{
				printf("[S->C] Auth Key\n");
				break;
			}
			default:
			printf("[S->C] Got unknown packet ID: %lu %02X\n",buff[0],buff[0]);
			return;
			break;
		}
		//printf("server sent Packet ID: %02X/%d\n",buff[0],buff[0]);
		fwrite(buff, len, 1, fd);

		for(int i=0;i<4;i++) {
			char c = 0xCC;
			fwrite(&c, 1, 1, fd);
			fflush(fd);
		}
		sendto(m_sd, (char *)buff, len, 0, (struct sockaddr *)&m_address_info, slen);
		//printf("Sending %d packet to client\n", len);
	}
}