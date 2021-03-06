
#ifndef __INC_MULTISOCK_H
#define __INC_MULTISOCK_H

//char *myIp = "192.168.99.100";
#define myIp	"0.0.0.0"
//char *peerIp = "192.168.99.100";
#define peerIp	"127.0.0.1"	
//#define peerIp	"192.168.99.1"	


/*** Thread Administration Message ***/
struct ThreadAdminMsg {
	int type;
	unsigned long info[3];
};
#define ADMIN_STOP_REQ	(1)
#define ADMIN_FIN		(2)
#define ADMIN_READY		(3)

#define PACKET_per_THREAD	(1000)
#define PEERS				(50)
#define PORT_OFFSET			(0)

struct SOCK_TBL {
	WSAOVERLAPPED Overlapped;
	struct x_sta{
		int send;
		int recv;
		int err;
		int deb;
	}p;
	struct x_sta c;
	int port;
	SOCKET sock;
	char *buf;
	WSABUF wbuf;
	int recvByte;
	int rcvF;
	MQ_ID callBackQ;
	void *pThread;
};

struct TH_TBL {
	int proto;
	int socks;
	int port;
	struct SOCK_TBL *st;
	MQ_ID cmqId;
	unsigned long lifetime;
};

SOCKET __cdecl crSock( int type, unsigned short port, BOOL server );
SOCKET __cdecl cSock( unsigned short port );
int __cdecl sockSendTo( SOCKET sock, char *buf, int packet_size, struct sockaddr_in *dest, LPWSAOVERLAPPED lpOverlapped );
void __cdecl setRecvBuf( SOCKET sock, int size );
void __cdecl setSendBuf( SOCKET sock, int size );

#endif	// __INC_MULTISOCK_H
