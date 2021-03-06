#include <stdio.h>
#include <winsock2.h>
#include <conio.h>
#include <ws2tcpip.h>
#include "wsyslib.h"
#include "evc.h"
#include "multisock.h"


SOCKET __cdecl crSock( int type, unsigned short port, BOOL server )
{
	struct sockaddr_in source;
	SOCKET sock;
	char tos = (char)0xfc;

	if ( type == SOCK_STREAM ) {
//		sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
		if ( sock != INVALID_SOCKET ) {
			source.sin_addr.s_addr = inet_addr(myIp);
			source.sin_family = AF_INET;
			source.sin_port = htons(port);
			if ( bind( sock, (struct sockaddr*)&source, sizeof( struct sockaddr_in ) ) ) {
				fprintf( stderr, "Failed to bind: %d\r\n", WSAGetLastError() );
				closesocket( sock );
				return INVALID_SOCKET;
			}
			if ( server ) {
				if ( listen( sock, SOMAXCONN ) ) {
					fprintf( stderr, "Failed to bind: %d\r\n", WSAGetLastError() );
					closesocket( sock );
					return INVALID_SOCKET;
				}

			}
		}
	}
	else if ( type == SOCK_DGRAM ) {
		return cSock( port );
	}
	else {
		return 0;
	}

	return sock;
}

SOCKET __cdecl cSock( unsigned short port )
{
	struct sockaddr_in source;
	SOCKET sock;
	char tos = (char)0xfc;

	sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, 0, WSA_FLAG_OVERLAPPED);
	if ( sock != INVALID_SOCKET ) {
		source.sin_addr.s_addr = inet_addr(myIp);
		source.sin_family = AF_INET;
		source.sin_port = htons(port);
		if ( bind( sock, (struct sockaddr*)&source, sizeof( struct sockaddr_in ) ) ) {
			fprintf( stderr, "Failed to bind: %d\r\n", WSAGetLastError() );
			closesocket( sock );
			return INVALID_SOCKET;
		}
	}
	/*	Windows2000以降
		ローカルコンピュータのHKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Tcpip\Parameters\に移動 
		DisableUserTOSSettingというキーを作成する(データ型は REG_DWORD). 
		DisableUserTOSSettingの内容を0にする. 
		レジストリエディタを終了し, マシンを再起動. 

		http://support.microsoft.com/default.aspx?scid=kb;ja;248611
  */
	if (setsockopt(sock, IPPROTO_IP, IP_TOS, (const char*)&tos, 
			sizeof(tos)) == SOCKET_ERROR) {
		fprintf( stderr, "Failed to Set TOS: %d\r\n", WSAGetLastError() );
	}
	return sock;
}

int send_pend;
int __cdecl sockSendTo( SOCKET sock, char *buf, int packet_size, struct sockaddr_in *dest, LPWSAOVERLAPPED lpOverlapped )
{
	int r;
	DWORD flags = 0;
	unsigned long bwrote;
	WSABUF wbuf;

	wbuf.len = packet_size;
	wbuf.buf = buf;
	r = WSASendTo( sock, &wbuf, 1, &bwrote, flags, (struct sockaddr*)dest, 
//		sizeof(struct sockaddr_in), lpOverlapped, SendCompletionROUTINE);
		sizeof(struct sockaddr_in), lpOverlapped, 0);
	if (r == SOCKET_ERROR) {
		int e = WSAGetLastError();
		if ( e != ERROR_IO_PENDING ) {
			fprintf( stderr, "send failed: %d\r\n", e );
			if ( e == WSAENOBUFS )	/* 10055 */
				Sleep(100);
			else
				return -1;
		}
		else
			send_pend++;
	}
#if 0
	r = sendto( sock, buf, packet_size, flags, 
				(struct sockaddr*)dest, sizeof(struct sockaddr_in) );
	if (r == SOCKET_ERROR) {
		int e = WSAGetLastError();
		fprintf( stderr, "send failed: %d\r\n", e ); 
		return -1;
	}
#endif
	return 0;
}

void __cdecl setRecvBuf( SOCKET sock, int size )
{
	int opt;
	int optlen = sizeof(int);
	if ( getsockopt( sock, SOL_SOCKET, SO_RCVBUF, (char*)&opt, &optlen ) != SOCKET_ERROR ) {
		if ( size > opt )
			setsockopt( sock, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(int) );
	}
}

void __cdecl setSendBuf( SOCKET sock, int size )
{
	int opt;
	int optlen = sizeof(int);
	if ( getsockopt( sock, SOL_SOCKET, SO_SNDBUF, (char*)&opt, &optlen ) != SOCKET_ERROR ) {
		if ( size > opt ) {
			setsockopt( sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(int) );
			getsockopt( sock, SOL_SOCKET, SO_SNDBUF, (char*)&opt, &optlen );
			printf( "Send Buf = %d\r\n", opt );
		}
	}
}
