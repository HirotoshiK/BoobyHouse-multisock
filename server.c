#include <stdio.h>
#include <winsock2.h>
#include <conio.h>
#include <ws2tcpip.h>
#include "wsyslib.h"
#include "evc.h"
#include "multisock.h"
#include "args.h"

#define SOCK_MAX	100

void __cdecl tcpThread( void *arg, struct ThreadAdminEvent *event )
{
	int *a = (int*)arg;
	struct SOCK_TBL *vsktbl = (struct SOCK_TBL *)a[0];
	int socks = a[1];
	MQ_ID r_mqId = a[2];
	MQ_ID c_mqId = a[3];
	struct SOCK_TBL *sk;
	unsigned short port = vsktbl->port;
	unsigned long st = GetTickCount();
	unsigned long lt = 0;
	struct ThreadAdminMsg msg;
	int len = 1500;
	int lf = 0;
	EVC_ID evId;
	int fromlen = sizeof( struct sockaddr_in );
	int sn;
	struct SOCK_TBL sktbl[SOCK_MAX];
	int skcount = socks;
	int mq_event_type = _countof(sktbl);
	
	ZeroMemory( sktbl, sizeof(sktbl) );
	evId = EVCONT_NEW( GET_MQ_EVENT( c_mqId ), mq_event_type, EVCONT_PRIORITY_ROTATE );		/* Add MsgQueue & Rolling Mode */

	printf( "tcp thread arg = %4d\r\n", vsktbl[0].port );
	sk = sktbl;
	for ( sn = 0 ; sn < socks ; sn++, sk++, vsktbl++ ) {
		SOCKET listensock;
		*sk = *vsktbl;
		listensock = crSock( SOCK_STREAM, (unsigned short)sk->port, TRUE );
		if ( listensock == INVALID_SOCKET ) {
			fprintf( stderr, "Socket Error\r\n" );

			return;
		}
		else {
			fprintf( stderr, "TCP Listen Port %d\n", sk->port );
		}
		sk->buf = (char*)malloc( len );
		ZeroMemory( &sk->Overlapped, sizeof(WSAOVERLAPPED) );
		sk->sock = listensock;
		sk->Overlapped.hEvent = WSACreateEvent();
		EVCONT_ADD( evId, sk->Overlapped.hEvent, sn );
		sk->c.send = 0;
		sk->c.recv = 0;
		WSAEventSelect( listensock, sk->Overlapped.hEvent, FD_ACCEPT );
	}
	msg.type = ADMIN_READY;
	msg.info[0] = sktbl[0].port;
	msg.info[1] = (unsigned long)sktbl[0].pThread;
	MQ_SEND( r_mqId, (char*)&msg, sizeof(msg), NO_WAIT );

	while ( 1 ) {
		int recvByte;
		sn = EVCONT_WAIT( evId, INFINITE );	/* イベント待ち */
		if ( sn >= 0 && sn < skcount ) {
			WSANETWORKEVENTS    events;

			sk = &sktbl[sn];
			if ( WSAEnumNetworkEvents( sk->sock, sk->Overlapped.hEvent, &events) 
					== SOCKET_ERROR) {
				int e = WSAGetLastError();
				fprintf( stderr, "WSAEnumNetworkEvents Error(D)! %d\r\n", e );
				break;
			}
			else {
				if (events.lNetworkEvents & FD_CLOSE) {
					fprintf( stderr, "Closed[Port.%d]#%d\n", sk->port, sn );
				}
				if (events.lNetworkEvents & FD_ACCEPT) {
					SOCKADDR_IN sa_out;
					int nLen = sizeof( SOCKADDR_IN );
					SOCKET Socket = WSAAccept( sk->sock, (SOCKADDR*)&sa_out, &nLen, NULL, (DWORD_PTR)NULL );
					if( Socket == INVALID_SOCKET ){
						fprintf( stderr, "WSAAccept()でエラー [ ErrorCode:%d ]\n", WSAGetLastError() );
					}
					else if ( skcount < _countof(sktbl) ) {
						struct SOCK_TBL *csk = &sktbl[skcount];
						csk->sock = Socket;
						csk->buf = (char*)malloc( len );
						ZeroMemory( &csk->Overlapped, sizeof(WSAOVERLAPPED) );
						csk->Overlapped.hEvent = WSACreateEvent();
						EVCONT_ADD( evId, csk->Overlapped.hEvent, skcount );
						csk->c.send = 0;
						csk->c.recv = 0;
						WSAEventSelect( csk->sock, csk->Overlapped.hEvent, FD_READ | FD_CLOSE );
						csk->port = sk->port;
						fprintf( stderr, "Accept[Port.%d]#%d\n", sk->port, skcount );
						skcount++;

					}
					else {
						fprintf( stderr, "Socket Table Full\n" );
					}
				}
				if (events.lNetworkEvents & FD_WRITE) 
					;
				if (events.lNetworkEvents & FD_READ) {
					struct sockaddr_in source;
					DWORD flags = 0;
					recvByte = recvfrom( sk->sock, sk->buf, len, flags, 
												(struct sockaddr*)&source , &fromlen );
					if ( recvByte > 0 ) {
						sk->c.recv++;
						if ( lf == 0 ) {
							st = GetTickCount();
							lf = 1;
						}
						/* 受信処理 */
						fprintf( stderr, "Receive[TCP-Port.%d]#%d %dbyte\n", sk->port, sn, recvByte );
						send( sk->sock, sk->buf, recvByte, 0 );
//						testThreadC2_Recv( sk, sk->buf, len, recvByte, &source );
					}
					else if ( recvByte == SOCKET_ERROR ) {
						int e = WSAGetLastError();
						if ( e == WSAEWOULDBLOCK ) {
							ResetEvent(sk->Overlapped.hEvent);
						}
						else
							break;
					}
					else {
						fprintf( stderr, "Connection Closed?\r\n" );
						break;
					}
				}
			}
		}
		else if ( sn == mq_event_type ) {	/* Message Queue */
			int r = MQ_RECV( c_mqId, (char*)&msg, sizeof(msg), NO_WAIT );
			if ( r > 0 && msg.type == ADMIN_STOP_REQ )
				break;
		}
		else {
			fprintf( stderr, "(C)EVCONT_WAIT Error %d\r\n" );
			break;
		}
	}

	lt = GetTickCount() - st;

	EVCONT_DEL( evId );
	sk = sktbl;
	for ( sn = 0 ; sn < skcount ; sn++, sk++ ) {

		WSACloseEvent(sk->Overlapped.hEvent);
		if ( sk->sock ) {
			closesocket( sk->sock );
		}
		free( sk->buf );
	}
	msg.type = ADMIN_FIN;
	msg.info[0] = port;
	msg.info[1] = (unsigned long)sktbl[0].pThread;
	msg.info[2] = lt;
	MQ_SEND( r_mqId, (char*)&msg, sizeof(msg), NO_WAIT );
}

void __cdecl udpThread( void *arg, struct ThreadAdminEvent *event )
{
	int *a = (int*)arg;
	struct SOCK_TBL *sktbl = (struct SOCK_TBL *)a[0];
	int socks = a[1];
	MQ_ID r_mqId = a[2];
	MQ_ID c_mqId = a[3];
	struct SOCK_TBL *sk;
	unsigned short port = sktbl->port;
	unsigned long st = GetTickCount();
	unsigned long lt = 0;
	struct ThreadAdminMsg msg;
	int len = 1500;
	int lf = 0;
	EVC_ID evId;
	int fromlen = sizeof( struct sockaddr_in );
	int sn;

	evId = EVCONT_NEW( GET_MQ_EVENT( c_mqId ), socks, EVCONT_PRIORITY_ROTATE );		/* Add MsgQueue & Rolling Mode */

	printf( "udp thread arg = %4d\r\n", sktbl[0].port );
	sk = sktbl;
	for ( sn = 0 ; sn < socks ; sn++, sk++ ) {
		SOCKET sock = crSock( SOCK_DGRAM, (unsigned short)sk->port, TRUE );
		if ( sock == INVALID_SOCKET ) {
			fprintf( stderr, "Socket Error\r\n" );

			return;
		}
		fprintf( stderr, "UDP Bind Port %d\n", sk->port );
		sk->buf = (char*)malloc( len );
		ZeroMemory( &sk->Overlapped, sizeof(WSAOVERLAPPED) );
		sk->sock = sock;
		sk->Overlapped.hEvent = WSACreateEvent();
		EVCONT_ADD( evId, sk->Overlapped.hEvent, sn );
		sk->c.send = 0;
		sk->c.recv = 0;
		WSAEventSelect( sock, sk->Overlapped.hEvent, FD_READ | FD_CLOSE );
	}
	msg.type = ADMIN_READY;
	msg.info[0] = (unsigned long)sktbl[0].port;
	msg.info[1] = (unsigned long)sktbl[0].pThread;
	MQ_SEND( r_mqId, (char*)&msg, sizeof(msg), NO_WAIT );

	while ( 1 ) {
		int recvByte;
		sn = EVCONT_WAIT( evId, INFINITE );	/* イベント待ち */
		if ( sn >= 0 && sn < socks ) {
			WSANETWORKEVENTS    events;

			sk = &sktbl[sn];
			if ( WSAEnumNetworkEvents( sk->sock, sk->Overlapped.hEvent, &events) 
					== SOCKET_ERROR) {
				int e = WSAGetLastError();
				fprintf( stderr, "WSAEnumNetworkEvents Error(D)! %d\r\n", e );
				break;
			}
			else {
				if (events.lNetworkEvents & FD_CLOSE) {
					fprintf( stderr, "FD_CLOSE(D)!\r\n" );
					break;
				}
				if (events.lNetworkEvents & FD_WRITE) 
					;
				if (events.lNetworkEvents & FD_READ) {
					struct sockaddr_in source;
					DWORD flags = 0;
					recvByte = recvfrom( sk->sock, sk->buf, len, flags, 
												(struct sockaddr*)&source , &fromlen );
					if ( recvByte > 0 ) {
						sk->c.recv++;
						if ( lf == 0 ) {
							st = GetTickCount();
							lf = 1;
						}
						/* 受信処理 */
						fprintf( stderr, "Receive[UDP-Port.%d]#%d %dbyte\n", sk->port, sn, recvByte );
						sendto( sk->sock, sk->buf, recvByte, 0, (struct sockaddr*)&source, fromlen );
					}
					else if ( recvByte == SOCKET_ERROR ) {
						int e = WSAGetLastError();
						if ( e == WSAEWOULDBLOCK ) {
							ResetEvent(sk->Overlapped.hEvent);
						}
						else
							break;
					}
					else {
						fprintf( stderr, "Connection Closed?\r\n" );
						break;
					}
				}
			}
		}
		else if ( sn == socks ) {	/* Message Queue */
			int r = MQ_RECV( c_mqId, (char*)&msg, sizeof(msg), NO_WAIT );
			if ( r > 0 && msg.type == ADMIN_STOP_REQ )
				break;
		}
		else {
			fprintf( stderr, "(C)EVCONT_WAIT Error %d\r\n" );
			break;
		}
	}

	lt = GetTickCount() - st;

	EVCONT_DEL( evId );
	sk = sktbl;
	for ( sn = 0 ; sn < socks ; sn++, sk++ ) {

		WSACloseEvent(sk->Overlapped.hEvent);
		closesocket( sk->sock );
		free( sk->buf );
	}
	msg.type = ADMIN_FIN;
	msg.info[0] = port;
	msg.info[1] = (unsigned long)sktbl[0].pThread;
	msg.info[2] = lt;
	MQ_SEND( r_mqId, (char*)&msg, sizeof(msg), NO_WAIT );
}

/* Begin Server Thread */
int beginSrv( int threads, struct TH_TBL *tht, MQ_ID mqId )
{
	int i, j;
	int arg[5];
	MQ_ID cmqId;
	int exec_c = 0;
	int loop = 1;
	int ready = 0;
//	int portn = sport;
//	int id = 0;

	for ( i = 0; i < threads ; i ++ ) {
		struct SOCK_TBL *sktbl = tht[i].st;
		int tt;
		for ( j = 0 ; j < tht[i].socks ; j++ ) {
			sktbl[j].port = tht[i].port + j;
			sktbl[j].pThread = (void*)&tht[i];
		}
		arg[0] = (int)sktbl;
		arg[1] = tht[i].socks;
		arg[2] = (int)mqId;
		cmqId = MQ_CREATE( 16, sizeof(struct ThreadAdminMsg) );
		arg[3] = (int)cmqId;
		tht[i].cmqId = cmqId;
		if ( tht[i].proto == SOCK_DGRAM ) {
			tt = NEW_THREAD( "UDP", udpThread , arg, sizeof(arg), No_AdminEvent, 0 );
		}
		else if ( tht[i].proto == SOCK_STREAM ) {
			tt = NEW_THREAD( "TCP", tcpThread , arg, sizeof(arg), No_AdminEvent, 0 );
		}
		else {
			tt = 0;
		}
		if ( tt == 0 ) {
			break;
		}
		exec_c++;
//		id += tht[i].socks;
	}
	printf( "                    Create %d Threads\r\n", exec_c );

	/* Ready待ち */
	while ( loop && ready < exec_c ) {
		struct ThreadAdminMsg msg;
		int r;
		
		r = MQ_RECV( mqId, (char*)&msg, sizeof(struct ThreadAdminMsg), 1000 );
		if ( r > 0 ) {
			if ( msg.type == ADMIN_READY ) {
				struct TH_TBL *th = (struct TH_TBL*)msg.info[1];
				ready++;
				printf( "                    %s thread Ready = %4d\r\n", 
												th->proto == SOCK_STREAM ? "TCP" : "UDP",
												msg.info[0] );
			}
		}
		if (_kbhit() > 0) {
			char c = _getch();
			switch (c) {
			case 'b':	/* break */
				loop = 0;
				break;
			}
		}
	}
	printf( "                    All Thread Ready\r\n" );

	return exec_c;
}

void finish_sv( int threads, struct TH_TBL tht[] )
{
	struct ThreadAdminMsg smsg;
	int i;

	for ( i = 0; i < threads ; i++ ) {
		smsg.type = ADMIN_STOP_REQ;
		MQ_SEND( tht[i].cmqId, (char*)&smsg, sizeof(struct ThreadAdminMsg), NO_WAIT );
	}
}

struct SRV_PRM {
	int sport;
	int threads;
	int socks;
};
/* Thread */
//void thread_server_sub( int threads, int socks, HANDLE eventX, int sport, int proto )
void thread_server_sub( int p1, int p2, HANDLE eventX )
{
	struct SRV_PRM *tcp_prm = (struct SRV_PRM *)p1;
	struct SRV_PRM *udp_prm = (struct SRV_PRM *)p2;
	static struct TH_TBL thtbl[2000];
	int i, j;
	MQ_ID mqId;
	int loop = 1;
	int exec_c;			// 実行スレッド数
	unsigned long lt = GetTickCount();
	int threadsx = tcp_prm->threads + udp_prm->threads;
	int key_en = 1;
//	struct SOCK_TBL *st = sktbl;

	if ( threadsx > _countof( thtbl ) ) {
		fprintf( stderr, "Total Threads Over Linit!\n" );
		SetEvent( eventX );
		return;
	}

	ZeroMemory( thtbl, sizeof(thtbl) );

	mqId = MQ_CREATE( threadsx * 2, sizeof(struct ThreadAdminMsg) );

	for ( i = 0 ; i < tcp_prm->threads ; i++ ) {
		thtbl[i].st = (struct SOCK_TBL*)malloc( sizeof(struct SOCK_TBL) * SOCK_MAX );
		ZeroMemory( thtbl[i].st, sizeof(struct SOCK_TBL) * SOCK_MAX );
		thtbl[i].proto = SOCK_STREAM;
		thtbl[i].socks = tcp_prm->socks;
		thtbl[i].port = tcp_prm->sport + i * tcp_prm->socks;
//		st += tcp_prm->socks;
	}
	for ( j = 0 ; j < tcp_prm->threads ; j++, i++ ) {
		thtbl[i].st = (struct SOCK_TBL*)malloc( sizeof(struct SOCK_TBL) * udp_prm->socks );
		ZeroMemory( thtbl[i].st, sizeof(struct SOCK_TBL) * udp_prm->socks );
		thtbl[i].proto = SOCK_DGRAM;
		thtbl[i].socks = udp_prm->socks;
		thtbl[i].port = udp_prm->sport + j * udp_prm->socks;
//		st += udp_prm->socks;
	}
	exec_c = beginSrv( threadsx, thtbl, mqId );
	Sleep(1000);

//	thtbl[threads].st = st;

	printf( "Break or End ( b=Break, q=Query )\r\n" );
	lt = GetTickCount();
	while (loop) {
		struct ThreadAdminMsg msg;
		int r;
		
		r = MQ_RECV( mqId, (char*)&msg, sizeof(struct ThreadAdminMsg), 1000 );
		if ( r > 0 ) {
			if ( msg.type == ADMIN_FIN ) {
				int port = (int)msg.info[0];
				struct TH_TBL *th = (struct TH_TBL*)msg.info[1];
				th->lifetime = msg.info[2];
				exec_c--;
				if ( exec_c < 1 ) {
					break;
				}
			}
		}
		if ( key_en && _kbhit() > 0) {
			char cx = _getch();
			switch (cx) {
			case 'x':	/* */
//				printf( "Remaining Executing Thread = %d, Total %d packets, Comp Sock = %d\r\n", 
//								exec_c, totalCount, compCount );
				printf( "Remaining Executing Thread = %d\n", exec_c );
				break;
			case 'q':	/* query */
				printf( "\r\n" );
				for ( j = 0; j < threadsx ; j++ ) {
					struct TH_TBL *t = &thtbl[j];
					for ( i = 0 ; i < t->socks ; i++ ) {
						printf( "[%3d] %d - %d : %d - %d\r\n", i, t->st[i].p.send, t->st[i].c.recv,
														t->st[i].c.send, t->st[i].p.recv );
					}
				}
				break;
			case 'b':	/* break */
				WSACancelBlockingCall();
				fprintf( stderr, "Break!\r\n" );
				finish_sv( threadsx, thtbl );
				key_en = 0;
				break;
			}
		}
	}

	printf( "\r\n" );
	{
		int eTotal = 0;
		for ( j = 0; j < threadsx ; j++ ) {
			struct TH_TBL *t = &thtbl[j];

			for ( i = 0 ; i < t->socks ; i++ ) {
				printf( "[%3d] %d - %d : %d - %d : %d, %d :", i, t->st[i].p.send, t->st[i].c.recv,
													t->st[i].c.send, t->st[i].p.recv,
													t->st[i].p.err, t->st[i].c.err );
				eTotal += (t->st[i].p.err + t->st[i].c.err);
				printf( " %d\r\n" , t->lifetime );
			}
	//		printf( "Total Error = %d / %d, Comp Sock = %d\r\n" , eTotal, totalCount, compCount );
		}
	}
	lt = GetTickCount() - lt;
	printf( "Exec Time = %d ticks\r\n", lt );

	MQ_DEL( mqId );
	for ( i = 0 ; i < threadsx ; i++ ) {
		MQ_DEL( thtbl[i].cmqId );
		free( thtbl[i].st );
	}
	SetEvent( eventX );
}

/* Create Thread Server */
void thread_server( struct SRV_PRM *tcp_prm, struct SRV_PRM *udp_prm )
{
	HANDLE event;
	int t;
	int count = 0;

	if ( tcp_prm->socks > 0 ) {
		event = WSACreateEvent();
//		t = THREAD_CRATE( "Servers" , (FUNCPTR)thread_server_sub, tcp_prm->threads, tcp_prm->socks, (int)event, tcp_prm->sport, SOCK_STREAM );
		t = THREAD_CRATE( "Servers" , (FUNCPTR)thread_server_sub, (int)tcp_prm, (int)udp_prm, (int)event, 0, 0 );
		count++;

		if ( THREAD_PRIORITY( t, THREAD_PRIORITY_TIME_CRITICAL ) == FALSE ) {
			fprintf( stderr, "Thread Priority Set Error\r\n" );
		}
	}

	WaitForSingleObject( event, INFINITE );

	CloseHandle( event );
}




/* Main */
void main( int argc, char* argv[] )
{
	WSADATA wsaData;
	char ch;
	struct SRV_PRM udp_prm;
	struct SRV_PRM tcp_prm;
	char *argr;
	char rx;
	int prm_sock = 0;

	ZeroMemory( &udp_prm, sizeof(udp_prm) );
	ZeroMemory( &tcp_prm, sizeof(udp_prm) );


	while ( (rx = getopts( "h:i:s:t:u:", argc, argv, &argr )) != '\0' ) {
		int err = 0;
		switch ( rx ) {
		case 'h':
			if ( is_numeric( argr ) != 10 ) {
				err = -1;
			}
			else {
				tcp_prm.threads = atoi( argr );
				if ( tcp_prm.threads > 0 ) {
					if ( tcp_prm.socks == 0 ) {
						tcp_prm.socks = 1;
					}
					if ( tcp_prm.sport == 0 ) {
						tcp_prm.sport = 79;				// TCP Default
					}
				}
			}
			break;
		case 'i':
			if ( is_numeric( argr ) != 10 ) {
				err = -1;
			}
			else {
				udp_prm.threads = atoi( argr );
				if ( udp_prm.threads > 0 ) {
					if ( udp_prm.socks == 0 ) {
						udp_prm.socks = 1;
					}
					if ( udp_prm.sport == 0 ) {
						udp_prm.sport = 1025;			// UDP Default
					}
				}
			}
			break;
		case 's':
			if ( is_numeric( argr ) != 10 ) {
				err = -1;
			}
			else {
				prm_sock = atoi( argr );
			}
			break;
		case 't': 
			if ( is_numeric( argr ) != 10 ) {
				err = -1;
			}
			else {
				tcp_prm.sport = atoi( argr );
			}
			break;
		case 'u':
			if ( is_numeric( argr ) != 10 ) {
				err = -1;
			}
			else {
				udp_prm.sport = atoi( argr );
			}
			break;
		default:
			err = -1;
			break;
		}

		if ( err ) {
			fprintf( stderr, "options: -h<tcp thread(s)> -i<udp thread(s)> -s<sockets per thread> -t<tcp start port> -u<ucp start port>\n" );
			return;
		}
	}
	if ( prm_sock > 0 ) {
		if ( tcp_prm.threads > 0 ) {
			tcp_prm.socks = prm_sock;
		}
		if ( udp_prm.threads > 0 ) {
			udp_prm.socks = prm_sock;
		}
	}

	THREAD_CONT_LIB_INIT();
	if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
		fprintf( stderr, "Failed to find Winsock 2.1 or better.\r\n" );
		return;
	}
	printf( "Description   : %s\r\n", wsaData.szDescription );
	printf( "System Status : %s\r\n", wsaData.szSystemStatus );

	printf( "multisock parameters\n" );
	printf( " TCP Threads    : %d\n", tcp_prm.threads );
	printf( "     Socks      : %d\n", tcp_prm.socks );
	printf( "     Start Port : %d\n", tcp_prm.sport );
	printf( " UDP Threads    : %d\n", udp_prm.threads );
	printf( "     Socks      : %d\n", udp_prm.socks );
	printf( "     Start Port : %d\n", udp_prm.sport );

	thread_server( &tcp_prm, &udp_prm );

	WSACleanup();
	fprintf( stderr, "Hit Return Key " );
	ch = getchar();
}
