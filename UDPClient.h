#ifndef _UDPCLIENT_H
#define _UDPCLIENT_H
#include "main.h"
class INetClient;
enum SAMPState {
	ESAMPState_PreInit,
	ESAMPState_InitChallenge,
	ESAMPState_WaitChallenge,
	ESAMPState_Connected,
};
class UDPClient : public INetClient {
public:
	UDPClient(int sd, struct sockaddr_in *si_other, uint32_t server_ip, uint16_t server_port);
	~UDPClient();
	void process_packet(char *buff, int len);
	int getServerSocket();
	void readServer();
	void process_game_packet(char *buff, int len, bool client_to_server);
private:
	struct sockaddr_in m_address_info;
	int m_sd;

	uint32_t m_server_ip;
	uint16_t m_server_port;

	struct sockaddr_in m_server_addr;
	int m_server_socket;

	struct sockaddr_in m_proxy_addr;

	SAMPState m_state;
};
#endif