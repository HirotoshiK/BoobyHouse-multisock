
#ifndef _Inc_WSYSLIB_H
#define _Inc_WSYSLIB_H

typedef unsigned long	MQ_ID;
typedef void*	WSA_MQ_ID;
typedef unsigned long	SEM;
typedef int 		(__cdecl *FUNCPTR) ();	   /* ptr to function returning int */

#ifndef NO_WAIT
#define WAIT_FOREVER	(-1)
#define NO_WAIT	(0)
#endif

typedef enum		/* SEM_B_STATE */
{
    SEM_EMPTY,			/* 0: semaphore not available */
    SEM_FULL			/* 1: semaphore available */
} SEM_STATE;



MQ_ID MQ_CREATE(int maxMsgs, int maxMsgLength );
HANDLE GET_MQ_EVENT( MQ_ID msgQId );
int MQ_DEL( MQ_ID msgQId );
int MQ_RECV(MQ_ID msgQId, char *buffer, UINT maxNBytes, int timeout);
int MQ_SEND(MQ_ID msgQId, char *buffer, UINT nBytes, int timeout);
WSA_MQ_ID WSA_MQ_CREATE(int maxMsgs, int maxMsgLength, WSAEVENT sockEvent);
int WSA_MQ_RECV(WSA_MQ_ID msgQId, char *buffer, UINT maxNBytes, int timeout);
int WSA_MQ_SEND(WSA_MQ_ID msgQId, char *buffer, UINT nBytes, int timeout);
int WSA_MQ_ONESHOT_TIMER( WSA_MQ_ID msgQId, unsigned int milisec );
void WSA_MQ_TIMER_KILL( WSA_MQ_ID msgQId );

SEM SEM_B_CREATE(int options, SEM_STATE initialState);
int SEM_DEL(SEM semId);
int SEM_GET(SEM semId, int timeout);
int SEM_REL(SEM semId);

#define GET_TICK()	GetTickCount()
#define RAND()	rand()

#define WSA_MQ_ERROR	(-2)
#define WSA_MQ_TIMER	(-1)
#define WSA_MQ_OVLAP	(0)

/* Thread Admin Event */
struct ThreadAdminEvent{
	int type;
	int id;
}event;
#define No_AdminEvent			(0)		/* No Event */
#define AdminEventTypeNormal	(1)		/* Normal Event */
#define AdminEventTypeMsgQ		(2)		/* Normal Message Queue */

typedef void (__cdecl *NewThreadFunc)(void *, struct ThreadAdminEvent* );
void THREAD_CONT_LIB_INIT();
int NEW_THREAD( char *name, NewThreadFunc func, int *arg, int argsize, int createEvent, int eventId );
struct ThreadAdminEvent *GET_THREAD_EVENT_HANDLE( int id );
BOOL THREAD_PRIORITY( int id, int nPriority );

int DELAY(int timeout /* ms */);
int THREAD_CRATE( char *name, FUNCPTR entryPt, 
				 int arg1, int arg2, int arg3, int arg4, int arg5);


#endif




