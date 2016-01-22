#include "INetClient.h"
#include "UDPClient.h"
#include "encryption.h"
#include <stdio.h>
#include <memory.h>

#include <BitStream.h>
#include <DS_RangeList.h>
#include <StringCompressor.h>


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
	ID_INVALID_PASSuint16_t,
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

/// These enumerations are used to describe when packets are delivered.
enum PacketPriority
{
	SYSTEM_PRIORITY,   /// \internal Used by RakNet to send above-high priority messages.
	HIGH_PRIORITY,   /// High priority messages are send before medium priority messages.
	MEDIUM_PRIORITY,   /// Medium priority messages are send before low priority messages.
	LOW_PRIORITY,   /// Low priority messages are only sent when no other messages are waiting.
	NUMBER_OF_PRIORITIES
};

/// These enumerations are used to describe how packets are delivered.
/// \note  Note to self: I write this with 3 bits in the stream.  If I add more remember to change that
enum PacketReliability
{
	UNRELIABLE = 6,   /// Same as regular UDP, except that it will also discard duplicate datagrams.  RakNet adds (6 to 17) + 21 bits of overhead, 16 of which is used to detect duplicate packets and 6 to 17 of which is used for message length.
	UNRELIABLE_SEQUENCED,  /// Regular UDP with a sequence counter.  Out of order messages will be discarded.  This adds an additional 13 bits on top what is used for UNRELIABLE.
	RELIABLE,   /// The message is sent reliably, but not necessarily in any order.  Same overhead as UNRELIABLE.
	RELIABLE_ORDERED,   /// This message is reliable and will arrive in the order you sent it.  Messages will be delayed while waiting for out of order messages.  Same overhead as UNRELIABLE_SEQUENCED.
	RELIABLE_SEQUENCED /// This message is reliable and will arrive in the sequence you sent it.  Out or order messages will be dropped.  Same overhead as UNRELIABLE_SEQUENCED.
};

/// Given a number of bits, return how many uint8_ts are needed to represent that.
#define BITS_TO_BYTES(x) (((x)+7)>>3)
#define BYTES_TO_BITS(x) ((x)<<3)

typedef struct _ONFOOT_SYNC_DATA
{
	uint16_t lrAnalog;
	uint16_t udAnalog;
	uint16_t wKeys;
	float vecPos[3];
	float fQuaternion[4];
	uint8_t uint8_tHealth;
	uint8_t uint8_tArmour;
	uint8_t uint8_tCurrentWeapon;
	uint8_t uint8_tSpecialAction;	
	float vecMoveSpeed[3];
	float vecSurfOffsets[3];
	uint16_t wSurfInfo;
	uint32_t	iCurrentAnimationID;
} ONFOOT_SYNC_DATA;


UDPClient::UDPClient(int sd, struct sockaddr_in *si_other, uint32_t server_ip, uint16_t server_port) {
	m_sd = sd;
	memcpy(&m_address_info, si_other, sizeof(m_address_info));

	m_server_addr.sin_family=AF_INET;  
    m_server_addr.sin_addr.s_addr=(server_ip);  
    m_server_addr.sin_port=htons(server_port);  

    m_server_socket = socket(AF_INET, SOCK_DGRAM, 0);

    m_server_port = server_port;
    m_server_ip = server_ip;

    m_state = ESAMPState_PreInit;
    m_raknet_mode = false;

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
	static bool writemode = false;
	static FILE *fd = NULL;
	if(!fd) {
		fd = fopen("dump.bin", "wb");
	}
	if(client_to_server) {
		sampDecrypt((uint8_t*)buff, len, (m_server_port), 0);
	}
	uint8_t msgid = 0;
	uint16_t data_len = 0;
	char data[256];
	if(!m_raknet_mode) {
		msgid = buff[0];
	} else {
		RakNet::BitStream is((unsigned char *)buff, len, false);
		bool hasacks;
		uint8_t reliability = 0;
		uint16_t seqid = 0;
		is.ReadBits((unsigned char *)&hasacks, 1);
		is.Read(seqid);
		//printf("**** Packet %d\n",seqid);
		if(hasacks) {
			DataStructures::RangeList<uint16_t> acknowlegements;
			acknowlegements.Deserialize(&is);
			//printf("**** Num acknowlegements: %d\n",acknowlegements.Size());
		}
		is.ReadBits(&reliability, 4, true);
		
		//printf("reliability: %d\n",reliability);
		if(reliability == UNRELIABLE_SEQUENCED || reliability == RELIABLE_SEQUENCED || reliability == RELIABLE_ORDERED ) {
			uint8_t orderingChannel = 0;
			uint16_t orderingIndexType;
			is.ReadBits((unsigned char *)&orderingChannel, 5, true);
			is.Read(orderingIndexType);
			printf("**** reliability: %d - (%d %d)\n",reliability,orderingChannel, orderingIndexType);
		}

		bool is_split_packet;
		//is.ReadBits((unsigned char *)&is_split_packet, 1);
		is.Read(is_split_packet);

		if(is_split_packet) {
			uint16_t split_packet_id;
			uint32_t split_packet_index, split_packet_count;
			is.Read(split_packet_id);
			is.ReadCompressed(split_packet_index);
			is.ReadCompressed(split_packet_count);
			printf("**** Split: (%d) %d %d\n", split_packet_id, split_packet_index, split_packet_count);
		}
		is.ReadCompressed(data_len);
		
		
		data_len = BITS_TO_BYTES(data_len);
		
		is.ReadAlignedBytes((uint8_t*)&data, data_len);
		msgid = data[0];
	}
	socklen_t slen = sizeof(struct sockaddr_in);
	if(data_len > len || data_len < 0)  {data_len = len-4;}
	RakNet::BitStream is((unsigned char *)&data[1], data_len, true);
	if(client_to_server) {		
		switch(msgid) {
			case ID_OPEN_CONNECTION_REQUEST:
				printf("[C->S] Open Connection Inquiry\n");
			break;
			case ID_CONNECTION_REQUEST: {
				printf("[C->S] Connection Request\n");
				break;
			}
			case ID_AUTH_KEY:
			{
				
				uint8_t key_len;
				char key[256];
				memset(&key,0,sizeof(key));
				is.Read(key_len);
				is.Read(key, key_len);
				printf("[C->S] Auth Key: %s\n",key);
				break;
			}
			case ID_NEW_INCOMING_CONNECTION: {
				printf("[C->S] New Connection\n");
				break;
			}
			case ID_CONNECTED_PONG: {
				//printf("[C->S] Pong\n");
				break;
			}
			case ID_RPC: {
				uint8_t rpcid;
				uint32_t bits;
				is.Read(rpcid);
				is.ReadCompressed(bits);
				uint32_t bytes = BYTES_TO_BITS(bits);

				
				if(rpcid == 0 ||data_len < 0 || data_len > len || bits == 0 || rpcid != 25) break;
				printf("[C->S] RPC %d (%d - %d - %d)\n",rpcid, bytes, bits, data_len);
				
				char rpcdata[1024];
				
				is.ReadBits((unsigned char *)&rpcdata, bits, false);
				RakNet::BitStream bs((unsigned char *)&rpcdata, bytes, true);
				if(rpcid == 25) {
					uint32_t netver;
					char name[24];
					uint32_t challenge;
					char version[24];
					uint8_t mod;
					char auth[4*16];
					uint8_t auth_len;
					memset(&auth,0,sizeof(auth));
					memset(&version,0,sizeof(auth));
					memset(&name,0,sizeof(name));
					uint8_t namelen;
					uint8_t verlen;

					uint32_t client_chal;

					bs.Read(netver);
					bs.Read(mod);
					bs.Read(namelen);
					bs.Read(name, namelen);
					bs.Read(client_chal);
					bs.Read(auth_len);
					bs.Read(auth, auth_len);
					bs.Read(verlen);
					bs.Read(version, verlen);

					printf("Name: %s\nGPCI: %s\nVersion: %s\n",name,auth,version);
				} else if(rpcid == 50) {
					uint32_t cmdlen;
					uint8_t cmd[256];
					bs.Read(cmdlen);
					memset(&cmd,0,sizeof(cmd));
					bs.Read((char *)&cmd, cmdlen);
					printf("Cmd: %s\n",cmd);

				}
				
				break;
			}
			case ID_PLAYER_SYNC: 
			ONFOOT_SYNC_DATA fs;
			is.Read((char *)&fs, sizeof(fs));
			//printf("[C->S] Pos: %f, %f, %f\n",fs.vecPos[0],fs.vecPos[1],fs.vecPos[2]);
			break;
			case ID_VEHICLE_SYNC: {
				//printf("[C->S] Vehicle Sync\n'");
				break;
			}
			case ID_STATS_UPDATE: {
				//printf("[C->S] Stats Update\n");
				break;
			}
			case ID_DISCONNECTION_NOTIFICATION: {
				printf("[C->S] Disconnecting\n");
				break;
			}
			case ID_INTERNAL_PING: {
				//printf("[C->S] Internal Ping\n");
				break;
			}
			default:
			//printf("[C->S] Got unknown packet ID: %lu %02X (len %d - %d)\n",msgid,msgid, data_len-1,len);
			//return;
			break;
		}
		//printf("client sent Packet ID: %02X/%d\n",buff[0],buff[0]);
		/*
		fwrite(buff, len, 1, fd);

		for(int i=0;i<4;i++) {
			char c = 0xFF;
			fwrite(&c, 1, 1, fd);
			fflush(fd);
		}
		*/
		
		sampEncrypt((uint8_t*)buff, len-1, (m_server_port), 0);		
		sendto(m_server_socket,(char *)buff,len,0,(struct sockaddr *)&m_server_addr, slen);

	} else {
		switch(msgid) {
			case ID_OPEN_CONNECTION_COOKIE: {
				printf("[S->C] Cookie Request\n");
				m_state = ESAMPState_InitChallenge;
				break;
			}
			case ID_OPEN_CONNECTION_REPLY:
			printf("[S->C] Open Connection Reply\n");
			if(m_state == ESAMPState_InitChallenge) {
				m_raknet_mode = true;
				m_state = ESAMPState_WaitChallenge;
				printf("Setting to wait challenge %d\n", m_state);
			}
			break;
			case ID_AUTH_KEY:
			{
				uint8_t key_len;
				char key[256];
				memset(&key,0,sizeof(key));
				is.Read(key_len);
				is.Read(key, key_len);
				printf("[S->C] Auth Key: %s\n", key);
				break;
			}
			case ID_CONNECTION_REQUEST_ACCEPTED: {
				printf("[S->C] Connection Accepted\n");
				is.IgnoreBits(32);
				is.IgnoreBits(16);
				uint16_t player_id;
				uint32_t challenge;
				is.Read(player_id);
				is.Read(challenge);
				printf("ID: %d, challenge: %08X\n",player_id, challenge);
				writemode = true;


				break;
			}
			case ID_RPC: {
				//93
				uint8_t rpcid;
				uint32_t bits;
				is.Read(rpcid);
				is.ReadCompressed(bits);
				uint32_t bytes = BYTES_TO_BITS(bits);
				
				
				if(rpcid != 93) break;
				printf("[S->C] RPC: %d\n",rpcid);
				char rpcdata[1024];
				
				is.ReadBits((unsigned char *)&rpcdata, bits, false);
				RakNet::BitStream bs((unsigned char *)&rpcdata, bytes, true);
				if(rpcid == 93) {
					char msg[256];
					memset(&msg, 0, sizeof(msg));
					uint32_t col, clen;
					bs.Read(col);
					bs.Read(clen);
					bs.Read((char *)&msg, clen);
					printf("Msg: [%08X] %s\n", col, msg);

				} else if(rpcid == 61) {
/*

struct stSAMPDialog
{
	int iIsActive;
	BYTE bDialogStyle;
	WORD wDialogID;
	BYTE bTitleLength;
	char szTitle[257];
	BYTE bButton1Len;
	char szButton1[257];
	BYTE bButton2Len;
	char szButton2[257];
	char szInfo[257];
};
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(sampDialog.wDialogID);
	bsData.Read(sampDialog.bDialogStyle);

	bsData.Read(sampDialog.bTitleLength);
	bsData.Read(sampDialog.szTitle, sampDialog.bTitleLength);
	sampDialog.szTitle[sampDialog.bTitleLength] = 0;

	bsData.Read(sampDialog.bButton1Len);
	bsData.Read(sampDialog.szButton1, sampDialog.bButton1Len);
	sampDialog.szButton1[sampDialog.bButton1Len] = 0;

	bsData.Read(sampDialog.bButton2Len);
	bsData.Read(sampDialog.szButton2, sampDialog.bButton2Len);
	sampDialog.szButton2[sampDialog.bButton2Len] = 0;

	stringCompressor->DecodeString(sampDialog.szInfo, 256, &bsData);
*/
				uint8_t style;
				uint16_t dialogid;
				uint8_t titlelen;
				uint8_t title[256];
				uint8_t buttonslen[2];
				uint8_t buttons[2][256];
				uint8_t content[256];

				memset(&content,0,sizeof(content));
				memset((char *)&buttons,0,sizeof(buttons));
				memset(&title,0,sizeof(title));
				bs.Read(dialogid);
				bs.Read(style);
				bs.Read(titlelen);
				bs.Read((char *)&title, titlelen);

				bs.Read(buttonslen[0]);
				bs.Read((char *)&buttons[0], buttonslen[0]);

				bs.Read(buttonslen[1]);
				bs.Read((char *)&buttons[1], buttonslen[1]);

				StringCompressor::Instance()->DecodeString((char *)&content,256,&bs);

				printf("%s\n%s\n",title, content);
				}
				break;
			}
			case ID_INTERNAL_PING: {
				//printf("[S->C] Internal Ping\n");
				break;
			}
			case ID_PLAYER_SYNC: {
				//printf("[S->C] Player Sync\n'");
				break;
			}
			case ID_VEHICLE_SYNC: {
				//printf("[S->C] Vehicle Sync\n'");
				break;
			}
			case ID_RPC_REPLY: {
				//printf("[S->C] RPC Reply\n");
				break;
			}
			default:
			//printf("[S->C] Got unknown packet ID: %lu %02X (len %d - %d)\n",msgid,msgid, data_len-1,len);
			//return;
			break;
		}
		//printf("server sent Packet ID: %02X/%d\n",buff[0],buff[0]);
		if(writemode) {
			fwrite(buff, len, 1, fd);

			for(int i=0;i<4;i++) {
				char c = 0xCC;
				fwrite(&c, 1, 1, fd);
				fflush(fd);
			}
		}
		
		sendto(m_sd, (char *)buff, len, 0, (struct sockaddr *)&m_address_info, slen);
		//printf("Sending %d packet to client\n", len);
	}
}