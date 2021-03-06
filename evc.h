
typedef unsigned long	EVC_ID;

/* Add Event */
int EVCONT_ADD( EVC_ID evId, HANDLE hEvent, int type );

/* Remoeve Event */
int EVCONT_REM( EVC_ID evId, int type );

/* Create New Control */
EVC_ID EVCONT_NEW( HANDLE hEvent, int type, int roll );
#define EVCONT_PRIORITY_ROTATE		(TRUE)
#define EVCONT_PRIORITY_FIXED		(FALSE)

/* Delete Control */
void EVCONT_DEL( EVC_ID evId );

/* Wait for Events */
int EVCONT_WAIT( EVC_ID evId, DWORD milisec );
int EVCONT_WAIT_IO( EVC_ID evId, DWORD milisec );
#define EVCONT_IO_COMPLETION		(0x80000000)
#define EVCONT_TIMEOUT				(-1)
#define EVCONT_ERROR				(-2)
#define EVCONT_OK					(0)

