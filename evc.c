#include <stdio.h>
#include <winsock2.h>
#include <conio.h>
#include <ws2tcpip.h>
#include "wsyslib.h"
#include "evc.h"


struct EVCTABLE {
	int *kind;
	int num;
	HANDLE *hEvent;
	SEM sem;		/* 排他制御 */
	int roll;
};

static int add_evt( struct EVCTABLE *box, HANDLE hEvent, int type ) 
{
	if ( box->num < WSA_MAXIMUM_WAIT_EVENTS ) {
		box->kind = realloc( box->kind, sizeof(int) * ( box->num + 1 ) );
		box->hEvent = realloc( box->hEvent, sizeof(HANDLE*) * ( box->num + 1 ) );
		box->kind[box->num] = type;
		box->hEvent[box->num] = hEvent;
		box->num++;
		return EVCONT_OK;
	}
	return EVCONT_ERROR;
}

static int rem_evt( struct EVCTABLE *box, int type ) 
{
	int n;
	int *k = box->kind;
	int c;
	
	for ( n = 0 ; n < box->num ; n++ ) {
		if ( *k == type )
			break;
		k++;
	}
	if ( n >= box->num )
		return EVCONT_ERROR;
	c = box->num - n - 1;
	if ( c > 0 ) {
		memcpy( k, k + 1, c * sizeof(int) );
		memcpy( box->hEvent[n], box->hEvent[n+1], c * sizeof(int) );
	}
	box->num--;
	box->kind = realloc( box->kind, sizeof(int) * box->num );
	box->hEvent = realloc( box->hEvent, sizeof(HANDLE*) * box->num );
	return EVCONT_OK;
}

/* Add Event */
int EVCONT_ADD( EVC_ID evId, HANDLE hEvent, int type ) 
{
	struct EVCTABLE *box = (struct EVCTABLE *)evId;
	int r;

	SEM_GET( box->sem, WAIT_FOREVER );	
	r = add_evt( box, hEvent, type );
	SEM_REL( box->sem );
	return r;
}

/* Remoeve Event */
int EVCONT_REM( EVC_ID evId, int type ) 
{
	struct EVCTABLE *box = (struct EVCTABLE *)evId;
	int r;

	SEM_GET( box->sem, WAIT_FOREVER );	
	r = rem_evt( box, type );
	SEM_REL( box->sem );
	return r;
}

/* Create New Control */
EVC_ID EVCONT_NEW( HANDLE hEvent, int type, int roll ) 
{
	struct EVCTABLE *box;

	if ( type < 0 )
		return (EVC_ID)NULL;
	box = (struct EVCTABLE *)malloc( sizeof(struct EVCTABLE) );
	if ( box ) {
		box->num = 0;
		box->kind = 0;
		box->hEvent = 0;
		if ( hEvent )
			add_evt( box, hEvent, type );
		box->sem = SEM_B_CREATE(0, SEM_FULL);
		box->roll = roll;
	}
	return (EVC_ID)box;
}

/* Delete Control */
void EVCONT_DEL( EVC_ID evId ) 
{
	struct EVCTABLE *box = (struct EVCTABLE *)evId;

	if ( box->kind )
		free( box->kind );
	if ( box->hEvent )
		free( box->hEvent );
	SEM_DEL( box->sem );
	free( box );
}

/* Event Shift */
int evc_sta[10];
static void shift_evc( struct EVCTABLE *box, int p )
{
	int b_kind;
	HANDLE *b_hEvent;
	int last = box->num - 1;
	int move;
#if DEBUG_ENABLE
	static int f;
	evc_sta[p]++;
	if ( p == 0 && f == 0 ) {
		int i;
		for ( i = 0 ; i < 5 ; i++ ) {
			printf( "[%d]%d ", i, evc_sta[i] );
		}
		printf( "\r\n" );
		f = 1;
	}
#endif
	if ( p == last )
		return;
	b_kind = box->kind[p];
	b_hEvent = box->hEvent[p];
	move = box->num - p - 1;
	memcpy( &box->kind[p], &box->kind[p+1], move * sizeof(int));
	memcpy( &box->hEvent[p], &box->hEvent[p+1], move * sizeof(HANDLE*) );
	box->kind[last] = b_kind;
	box->hEvent[last] = b_hEvent;
}

/* Wait for Events */
int EVCONT_WAIT( EVC_ID evId, DWORD milisec )
{
	struct EVCTABLE *box = (struct EVCTABLE *)evId;
	int rc;
	int n;

	if ( box->num < 1 )
		return EVCONT_ERROR;

	SEM_GET( box->sem, WAIT_FOREVER );	
	rc = (int)WSAWaitForMultipleEvents( box->num, box->hEvent, FALSE,
							milisec, FALSE);	/* イベント待ち */
	if (rc == WSA_WAIT_TIMEOUT) {
		SEM_REL( box->sem );
		return EVCONT_TIMEOUT;
	}
	n = box->kind[rc];
	if ( box->roll == EVCONT_PRIORITY_ROTATE )
		shift_evc( box, rc );
	SEM_REL( box->sem );
	return n;
}

/* Wait for Events or I/O Complete */
int EVCONT_WAIT_IO( EVC_ID evId, DWORD milisec )
{
	struct EVCTABLE *box = (struct EVCTABLE *)evId;
	int rc;
	int n;

	if ( box->num < 1 )
		return EVCONT_ERROR;

	SEM_GET( box->sem, WAIT_FOREVER );	
	rc = (int)WSAWaitForMultipleEvents( box->num, box->hEvent, FALSE,
							milisec, TRUE);	/* イベント待ち */
	if (rc == WSA_WAIT_TIMEOUT) {
		SEM_REL( box->sem );
		return EVCONT_TIMEOUT;
	}
	if ( rc == WAIT_IO_COMPLETION )
		n = EVCONT_IO_COMPLETION;
	else {
		n = box->kind[rc];
		if ( box->roll == EVCONT_PRIORITY_ROTATE )
			shift_evc( box, rc );
	}
	SEM_REL( box->sem );
	return n;
}


