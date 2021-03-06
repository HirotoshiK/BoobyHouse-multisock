#include <stdio.h>
#include <process.h>
#include <string.h>
#include <winsock2.h>
#include <stdlib.h>
//#include <iostream.h>
#include "wsyslib.h"

/******************************************************
	Message Queue Library
******************************************************/
//MQ_ID msgqcnt = 0;
unsigned long timerticks;

//#define MBOXSIZE 512
//#define MBOXLEN	512
struct MSG_Q_BOX {
	int type;
	int wp;
	int rp;
	int msgs;
	int length;
	union {
		HANDLE norm;
		struct asyn_event {
			WSAEVENT event[3];
			unsigned long timerId;
			WSAOVERLAPPED overLapped;
		}wsa;
	}event;
	SEM sem;		/* 排他制御 */
	char *box;		/* メッセージ格納BOX */
	int *blen;		/* メッセージ長テーブル */
	int count;
	int wr_count;
	int rd_count;
};
#define MQ_TYPE_NORMAL	(0x12345678)
#define MQ_TYPE_WSA_EV	(0xabcdef00)

#define WSA_EVENT_NUM_MQ	(0)		/* Message */
#define WSA_EVENT_NUM_TIMER	(1)		/* Timer Event */
#define WSA_EVENT_NUM_OVLAP	(2)		/* OverLaped */

/* Create Message Queue */
static struct MSG_Q_BOX *mq_create( int type, int maxMsgs, int maxMsgLength ) 
{
	struct MSG_Q_BOX *qbox;
	int box_size = (maxMsgs+1) * maxMsgLength;

	qbox = (struct MSG_Q_BOX *)malloc( sizeof(struct MSG_Q_BOX) );
	if ( qbox == NULL )
		return 0;
	qbox->box = (char*)malloc( box_size );
	if ( qbox->box == NULL ) {
		free( qbox );
		return 0;
	}
	qbox->blen = (int*)malloc( (maxMsgs+1) * sizeof(int) );
	if ( qbox->blen == NULL ) {
		free( qbox->box );
		free( qbox );
		return 0;
	}
	qbox->type = type;
	qbox->wp = 0;
	qbox->rp = 0;
	qbox->length = maxMsgLength;
	qbox->msgs = maxMsgs + 1;
	if ( type == MQ_TYPE_WSA_EV ) {
		qbox->event.wsa.event[WSA_EVENT_NUM_OVLAP] = 0;
		qbox->event.wsa.event[WSA_EVENT_NUM_MQ] = WSACreateEvent();
		qbox->event.wsa.event[WSA_EVENT_NUM_TIMER] = WSACreateEvent();
		qbox->event.wsa.timerId = 0;
	}
	else {	/* normal */
		qbox->event.norm = CreateEvent(0, TRUE, FALSE, 0);
	}
	qbox->sem = SEM_B_CREATE(0, SEM_FULL);
	if ( qbox->sem == 0 ) {
		MQ_DEL((MQ_ID)qbox);
		return 0;
	}
	qbox->count = 0;
	qbox->wr_count = 0;
	qbox->rd_count = 0;

	return qbox;
}
/* Get Normal Message Queue Event */
HANDLE GET_MQ_EVENT( MQ_ID msgQId )
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	return qbox->event.norm;
}
/* Create Normal Message Queue */
MQ_ID MQ_CREATE(int maxMsgs, int maxMsgLength)
{
	return (MQ_ID)mq_create(MQ_TYPE_NORMAL, maxMsgs, maxMsgLength);
}
/* Create WSA Message Queue */
WSA_MQ_ID WSA_MQ_CREATE(int maxMsgs, int maxMsgLength, WSAEVENT sockEvent)
{
	struct MSG_Q_BOX *qbox = mq_create(MQ_TYPE_WSA_EV, maxMsgs, maxMsgLength);
	if ( qbox ) {
		qbox->event.wsa.event[WSA_EVENT_NUM_OVLAP] = sockEvent;
		qbox->event.wsa.overLapped.hEvent = sockEvent;
	}
	return (WSA_MQ_ID)qbox;
}

/* Delete Message Queue */
static int mq_del( struct MSG_Q_BOX *qbox )
{
	SEM_DEL( qbox->sem );	/* 2005/7/6修正 */
	free( qbox->blen );
	free( qbox->box );
	free( qbox );
	return 0;
}
/* Delete Normal Message Queue */
int MQ_DEL( MQ_ID msgQId )
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	if ( qbox->type != MQ_TYPE_NORMAL )
		return -1;
	return mq_del( qbox );
}
/* Delete WSA Message Queue */
int WSA_MQ_DEL( WSA_MQ_ID msgQId )
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	if ( qbox->type != MQ_TYPE_WSA_EV )
		return -1;
	return mq_del( qbox );
}

/* Receive from Normal Message Queue */
int MQ_RECV(MQ_ID msgQId, char *buffer, UINT maxNBytes, int timeout/* ms */)
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	DWORD milisec;
	int rc;
	int xrp;
	char *ptr;

	if ( qbox->type != MQ_TYPE_NORMAL )
		return -1;
	/* 排他 */
	SEM_GET( qbox->sem, WAIT_FOREVER );	

	xrp = qbox->rp + 1;
	if (xrp >= qbox->msgs)
		xrp = 0;

	if (qbox->rp == qbox->wp) { /* No Message */
		if (timeout == WAIT_FOREVER)
			milisec = INFINITE;
		else
			milisec = timeout;

		if (milisec==0) {
			ResetEvent(qbox->event.norm);
			SEM_REL( qbox->sem );	/* 排他 */
			return -1;
		}

		while (1) {
			SEM_REL( qbox->sem );	/* 排他 */
			rc = (int)WaitForSingleObject(qbox->event.norm, milisec);	/* イベント待ち */
			if (rc != WAIT_OBJECT_0) {
				timerticks += timeout;
				return -1;
			}
			ResetEvent(qbox->event.norm);
			SEM_GET( qbox->sem, WAIT_FOREVER );	
			if (qbox->rp != qbox->wp) {
				break;
			}
		}
	}

	ptr = &qbox->box[qbox->length * qbox->rp];
	rc = qbox->blen[qbox->rp];
	memcpy(buffer, ptr, rc);

	qbox->count--;

	qbox->rp = xrp;
	if (qbox->rp == qbox->wp) /* Empty */
		ResetEvent(qbox->event.norm);

	qbox->rd_count++;
	SEM_REL( qbox->sem );

	return rc;
}

/* Send to Normal Message Queue */
int MQ_SEND(MQ_ID msgQId, char *buffer, UINT nBytes, int timeout)
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	int xwp;
	char *ptr;

	if ( qbox->type != MQ_TYPE_NORMAL )
		return -1;
	SEM_GET( qbox->sem, WAIT_FOREVER );

	xwp = qbox->wp + 1;
	if (xwp >= qbox->msgs)
		xwp = 0;

	if (xwp == qbox->rp) {
		SEM_REL( qbox->sem );
		return -1;
	}

	ptr = &qbox->box[qbox->length * qbox->wp];
	memcpy(ptr, buffer, nBytes);
	qbox->blen[qbox->wp] = nBytes;

//	printf( "MsgSend [%d]\r\n", msgQtable[msgQId].wp);

	qbox->wp = xwp;
	qbox->count++;
	qbox->wr_count++;

	SEM_REL( qbox->sem );

	SetEvent(qbox->event.norm);

	return 0;
}

/* Receive from WSA Message Queue */
int WSA_MQ_RECV(WSA_MQ_ID msgQId, char *buffer, UINT maxNBytes, int timeout/* ms */)
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	DWORD milisec;
	int rc;
	int xrp;
	char *ptr;

	if ( qbox->type != MQ_TYPE_WSA_EV )
		return -1;
	/* 排他 */
	SEM_GET( qbox->sem, WAIT_FOREVER );	

	xrp = qbox->rp + 1;
	if (xrp >= qbox->msgs)
		xrp = 0;

	if (qbox->rp == qbox->wp) { /* No Message */
		int cEvents;
		if (timeout == WAIT_FOREVER)
			milisec = INFINITE;
		else
			milisec = timeout;

		if (milisec==0) {
			SEM_REL( qbox->sem );	/* 排他 */
			return WSA_MQ_ERROR;
		}
		if ( qbox->event.wsa.event[WSA_EVENT_NUM_OVLAP] )
			cEvents = WSA_EVENT_NUM_OVLAP + 1;
		else
			cEvents = WSA_EVENT_NUM_OVLAP;

		while (1) {
			SEM_REL( qbox->sem );	/* 排他 */
			rc = (int)WSAWaitForMultipleEvents(cEvents, qbox->event.wsa.event, FALSE,
							milisec, FALSE);	/* イベント待ち */
			if (rc == WSA_WAIT_TIMEOUT) {
				timerticks += timeout;
				return WSA_MQ_ERROR;
			}
			if ( rc == WSA_WAIT_EVENT_0 + WSA_EVENT_NUM_MQ ) {				/* MQ Event */
				WSAResetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_MQ]);
				SEM_GET( qbox->sem, WAIT_FOREVER );	
				if (qbox->rp != qbox->wp) {
					break;
				}
			}
			if ( rc == WSA_WAIT_EVENT_0 + WSA_EVENT_NUM_TIMER ) {			/* Timer Event */
				WSAResetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_TIMER]);
				return WSA_MQ_TIMER;
			}
			else if ( rc == WSA_WAIT_EVENT_0 + WSA_EVENT_NUM_OVLAP ) {	/* Socket Select Event */
//				WSAResetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_OVLAP]);
				return WSA_MQ_OVLAP;
			}
			else {

			}
		}
	}

	ptr = &qbox->box[qbox->length * qbox->rp];
	rc = qbox->blen[qbox->rp];
	memcpy(buffer, ptr, rc);

	qbox->count--;
	qbox->rp = xrp;
	qbox->rd_count++;
	SEM_REL( qbox->sem );

	if ( rc < 1 )
		return -1;
	return rc;
}
/* test */
int WSA_MQ_RECV_X(WSA_MQ_ID msgQId, char *buffer, UINT maxNBytes, int timeout/* ms */)
{
	int cEvents;
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	int rc;

	if ( qbox->event.wsa.event[WSA_EVENT_NUM_OVLAP] )
		cEvents = WSA_EVENT_NUM_OVLAP + 1;
	else
		cEvents = WSA_EVENT_NUM_OVLAP;

	WSASetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_MQ]);
	while (1) {
		SEM_REL( qbox->sem );	/* 排他 */
		rc = (int)WSAWaitForMultipleEvents(cEvents, qbox->event.wsa.event, FALSE,
						INFINITE, FALSE);	/* イベント待ち */
		if (rc == WSA_WAIT_TIMEOUT) {
			timerticks += timeout;
//			return WSA_MQ_ERROR;
		}
		if ( rc == WSA_WAIT_EVENT_0 + WSA_EVENT_NUM_MQ ) {				/* MQ Event */
			WSAResetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_MQ]);
			printf( "WSA_EVENT_NUM_MQ\r\n" );
		}
		if ( rc == WSA_WAIT_EVENT_0 + WSA_EVENT_NUM_TIMER ) {			/* Timer Event */
			WSAResetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_TIMER]);
			printf( "WSA_EVENT_NUM_TIMER\r\n" );
//			return WSA_MQ_TIMER;
		}
		else if ( rc == WSA_WAIT_EVENT_0 + WSA_EVENT_NUM_OVLAP ) {	/* Socket Select Event */
//			WSAResetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_OVLAP]);
			printf( "WSA_EVENT_NUM_OVLAP\r\n" );
			return WSA_MQ_OVLAP;
		}
		else {

		}
	}
	return 0;
}
/* Send to WSA Message Queue */
int WSA_MQ_SEND(WSA_MQ_ID msgQId, char *buffer, UINT nBytes, int timeout)
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	int xwp;
	char *ptr;

	if ( qbox->type != MQ_TYPE_WSA_EV )
		return -1;
	SEM_GET( qbox->sem, WAIT_FOREVER );

	xwp = qbox->wp + 1;
	if (xwp >= qbox->msgs)
		xwp = 0;

	if (xwp == qbox->rp) {
		SEM_REL( qbox->sem );
		return -1;
	}

	ptr = &qbox->box[qbox->length * qbox->wp];
	memcpy(ptr, buffer, nBytes);
	qbox->blen[qbox->wp] = nBytes;

//	printf( "MsgSend [%d]\r\n", msgQtable[msgQId].wp);

	qbox->wp = xwp;
	qbox->count++;
	qbox->wr_count++;

	SEM_REL( qbox->sem );

	WSASetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_MQ]);

	return 0;
}

/******************************************************
	Semaphore Library
******************************************************/
/* Binary Semaphore */
SEM SEM_B_CREATE(int options, SEM_STATE initialState)
{
	HANDLE semid;

	if (initialState==SEM_EMPTY)
		semid = CreateSemaphore(0, 0, 1, 0);
	else
		semid = CreateSemaphore(0, 1, 1, 0);

	return (SEM)semid;
}

/* Release Semaphore */
int SEM_REL(SEM semId)
{
	long semacount;
	BOOL rc;

	rc = ReleaseSemaphore((HANDLE)semId, 1, &semacount);
	if ( !rc ) {
		printf( "sem rel fail\r\n", semId );
		return -1;
	}
//	printf( "sem rel %08x\r\n", semId );
	return 0;
}

/* Get Semaphore */
int SEM_GET(SEM semId, int timeout /* ms */)
{
	int milisec;
	int rc;

	if (timeout == WAIT_FOREVER)
		milisec = INFINITE;
	else
		milisec = timeout;

#if 0
	rc = (int)WaitForSingleObject((HANDLE)semId, 0);
	if (rc != WAIT_OBJECT_0) {
//		printf("Semaphore!!\n");
	}
	else 
		return 0;
#endif

//	printf( "sem get %08x\r\n", semId );
	rc = (int)WaitForSingleObject((HANDLE)semId, milisec);
	if (rc != WAIT_OBJECT_0) {
#if 0
		printf("Semaphore timeout\n");
#endif
		return -1;
	}
	else if ( rc == 0xffffffff ) {
		printf( "sem get fail\r\n", semId );
		return -1;
	}

	return 0;
}

/* Delete Semaphore */
int SEM_DEL(SEM semId)
{
	CloseHandle( (HANDLE)semId );
	return 0;
}

/******************************************************
	Thread Library
******************************************************/
int DELAY(int timeout /* ms */)
{
	Sleep(timeout);
	return 0;
}

#define MaxThreadArgSize	( 10 * sizeof(int) )

/*** Create New Thread Parameter Header ***/
struct NewThreadParamHead {
	NewThreadFunc func;
	struct ThreadAdminTable *tt;
	int argsize;
};

/*** Create New Thread Parameter ***/
struct NewThreadParam {
	struct NewThreadParamHead hd;
	int dmy[1];
};

/*** Thread Administration Info ***/
struct ThreadAdminInfo {
	char name[16];
	unsigned long hndl;
	struct ThreadAdminEvent event;
};

/*** Thread Administration Table ***/
struct ThreadAdminTable {
	struct {
		struct ThreadAdminTable *next;
		struct ThreadAdminTable *prev;
	}link;
	struct ThreadAdminInfo info;
};
struct ThreadAdminTable threadTableTop;

/*** Thread Administration Message ***/
struct ThreadAdminMsg {
	int type;
};
#define ADMIN_STOP_REQ	(1)
#define ADMIN_STOP_REP	(2)

/*** Functions ***/
void freeThread( struct ThreadAdminTable *t );


/*** Start of Thread ***/
void __cdecl newThreadStart( void *prm )
{
	struct NewThreadParam *arg = prm;

	if ( arg->hd.argsize > MaxThreadArgSize )
		;
	else if ( arg->hd.argsize > 0 )
		arg->hd.func( arg->dmy, &arg->hd.tt->info.event );
	else
		arg->hd.func( 0, &arg->hd.tt->info.event );

	freeThread( arg->hd.tt );
	free( arg );
	_endthread();
}

/*** Alloc Thread Table ***/
struct ThreadAdminTable *allocThread( int createEvent, int id )
{
	struct ThreadAdminTable *t = malloc( sizeof(struct ThreadAdminTable) );

	if ( t == 0 )
		return 0;

	t->link.prev = threadTableTop.link.prev;	/* Bottomに結合 */
	if ( threadTableTop.link.prev == 0 ) {		/* 元のBottomが０ */
		threadTableTop.link.next = threadTableTop.link.prev = t;
	}
	t->link.next = 0;
	threadTableTop.link.prev = t;

	t->info.hndl = 0;
	t->info.event.type = createEvent;
	switch ( createEvent ) {
	case No_AdminEvent:
		t->info.event.id = (int)WSA_INVALID_EVENT;
		break;
	case AdminEventTypeNormal:
		t->info.event.id = (int)WSACreateEvent();
		if ( t->info.event.id == (int)WSA_INVALID_EVENT ) {
			freeThread( t );
			t = 0;
		}
		break;
	case AdminEventTypeMsgQ:		/* Normal Message Queue */
		t->info.event.id = id;
		break;
	}
	return t;
}

/*** Free Thread Table ***/
void freeThread( struct ThreadAdminTable *t )
{
	if ( t == 0 )
		return;

	if ( t->info.event.type == AdminEventTypeNormal && t->info.event.id != (int)WSA_INVALID_EVENT ) {
		WSACloseEvent((HANDLE)t->info.event.id);
	}

	if ( t->link.prev )							/* 前がある場合は */
		(t->link.prev)->link.next = t->link.next;
	else										/* 前がない場合は */
		threadTableTop.link.next = t->link.next;	/* Top更新 */

	if ( t->link.next )
		(t->link.next)->link.prev = t->link.prev;
	else
		threadTableTop.link.prev = t->link.prev;

	free( t );
}

/*** Create New Thread ( argsize <= MaxThreadArgSize ) ***/
int NEW_THREAD( char *name, NewThreadFunc func, int *arg, int argsize, int createEvent, int eventId )
{
	struct NewThreadParam *arg_area;
	struct ThreadAdminTable *tt;
	
	tt = allocThread( createEvent, eventId );
	if ( tt == 0 )
		return 0;

	if ( argsize >= 0 ) {
		arg_area = malloc( argsize + sizeof( struct NewThreadParamHead ) );
		if ( arg_area == 0 ) {
			goto nt_error;
		}
		arg_area->hd.func = func;
		arg_area->hd.argsize = argsize;
		arg_area->hd.tt = tt;
		if ( arg && argsize > 0 )
			memcpy( (char*)arg_area + sizeof( struct NewThreadParamHead ), arg, argsize );
		tt->info.hndl = _beginthread( newThreadStart, 0, arg_area );
		if ( tt->info.hndl < 0 ) {
			free( arg_area );
			goto nt_error;
		}
		return (int)tt;
	}

nt_error:
	/* tt解放 */
	freeThread( tt );
	return 0;
}
/*** Set Thread Priority ***/
BOOL THREAD_PRIORITY( int id, int nPriority )
{
	struct ThreadAdminTable *tt = (struct ThreadAdminTable *)id;

	return SetThreadPriority( (HANDLE)tt->info.hndl, nPriority );
}

/*** Get Event Handle for Thread ***/
struct ThreadAdminEvent *GET_THREAD_EVENT_HANDLE( int id )
{
	struct ThreadAdminTable *tt = (struct ThreadAdminTable *)id;
	return &tt->info.event;
}

void THREAD_CONT_LIB_INIT()
{
	memset( &threadTableTop, 0, sizeof(struct ThreadAdminTable) );
}

typedef void (*CT_FUNCPTR)( 
			int arg1, 
			int arg2, 
			int arg3, 
			int arg4, 
			int arg5 
);	   /* ptr to function returning int */

struct cThread_tbl {
    LPTHREAD_START_ROUTINE lpStartAddress;
	int arg[5];
};

static void cThread( struct cThread_tbl *param )
{
	CT_FUNCPTR func = (CT_FUNCPTR)param->lpStartAddress;

	func( param->arg[0], param->arg[1], param->arg[2], param->arg[3], param->arg[4] );
	free(param);
}

int THREAD_CRATE_OLD( char *name, FUNCPTR entryPt, 
				 int arg1, int arg2, int arg3, int arg4, int arg5)
{
	HANDLE SCH_handle;
	DWORD SCH_threadID;
	struct cThread_tbl *ct;

	ct = malloc( sizeof(struct cThread_tbl) );
	ct->lpStartAddress = (LPTHREAD_START_ROUTINE)entryPt;
	ct->arg[0] = arg1;
	ct->arg[1] = arg2;
	ct->arg[2] = arg3;
	ct->arg[3] = arg4;
	ct->arg[4] = arg5;

	SCH_handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)cThread, (void*)ct, 0 , &SCH_threadID);
	return (int)SCH_handle;
}


/*** Create Thread ***/
static void __cdecl newThreadStart2( int *arg, HANDLE h )
{
	CT_FUNCPTR func = (CT_FUNCPTR)arg[0];
	func( arg[1], arg[2], arg[3], arg[4], arg[5] );
}

int THREAD_CRATE( char *name, FUNCPTR entryPt, 
				 int arg1, int arg2, int arg3, int arg4, int arg5)
{
	int arg[6];

	arg[0] = (int)entryPt;
	arg[1] = arg1;
	arg[2] = arg2;
	arg[3] = arg3;
	arg[4] = arg4;
	arg[5] = arg5;
	return NEW_THREAD( name, newThreadStart2, arg, sizeof(arg), FALSE, 0 );
}


/******************************************************
	MQ Timer Library
******************************************************/
void CALLBACK timer_callback(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)dwUser;

	if ( qbox->event.wsa.timerId ) {
		WSASetEvent(qbox->event.wsa.event[WSA_EVENT_NUM_TIMER]);
		qbox->event.wsa.timerId = 0;
	}
}

int WSA_MQ_ONESHOT_TIMER( WSA_MQ_ID msgQId, unsigned int milisec )
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	MMRESULT mr;
	UINT resolution = 10; 
	
	mr = timeSetEvent( milisec, resolution, timer_callback, (DWORD)msgQId, TIME_ONESHOT );
	if ( mr ) {
		qbox->event.wsa.timerId = mr;
		return 0;
	}
	return -1;		/* error */													/* winmm.lib */
}

void WSA_MQ_TIMER_KILL( WSA_MQ_ID msgQId )
{
	struct MSG_Q_BOX *qbox = (struct MSG_Q_BOX *)msgQId;
	if ( qbox->event.wsa.timerId ) {
		timeKillEvent( qbox->event.wsa.timerId );
		qbox->event.wsa.timerId = 0;
	}
}



