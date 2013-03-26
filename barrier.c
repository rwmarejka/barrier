/*
%  cc -DUNITTEST=0 -I. -mt -Xa -v -c -O barrier.c
%t cc -DUNITTEST=1 -I. -mt -Xa -v -g -o barrier barrier.c -lpthread -lthread
%l cc -I. -mt -Xa -v -xO4 -G -Kpic -z defs -o barrier.so barrier.c -lthread -lc
 *
 * BARRIER(3T) - implement a barrier mechanism.
 *
 *	int	barrier_init( barrier_t *bp, int count, int type, void *arg );
 *	int	barrier_wait( barrier_t *bp );
 *	int	barrier_destroy( barrier_t *bp );
 *
 * written : Richard.Marejka@Sun.COM 1995.07.29
 * modified: Richard.Marejka@Sun.COM 2001.03.02
 *		- error detection
 *		- pshared member added for semaphore usage
 *		- compile-time choice of Unix Intl or POSIX semaphore.
 */

#pragma ident "@(#) barrier.c 1.16 02/06/10 Richard.Marejka@Sun.COM"

/* Feature Test Macros	*/

#define BARRIER_VERSION     (7)

/* Include Files	*/

#include <barrier.h>
#include <assert.h>

/* Constants & Macros	*/

/* Data Declarations	*/

/* P R I V A T E   I N T E R F A C E S			*/

#if !defined(__sun)

/*
 * If we are not on a Sun system (i.e. Solaris), then map the Solaris
 * semaphore interfaces to the POSIX semaphore interfaces. Make the
 * interfaces private (read: static) so as not to pollute the ABI of
 * wherever we are running (i.e. not Solaris).
 */

static int
sema_init( sema_t *sp, unsigned value, int type, void *arg ) {
	int	pshared;

	switch ( type ) {
	  case USYNC_PROCESS	:
		pshared	= PTHREAD_SCOPE_SYSTEM;
		break;

	  case USYNC_THREAD	:
		pshared	= PTHREAD_SCOPE_PROCESS;
		break;

	  default		:
		return( EINVAL );
		/*NOTREACHED*/
	}

	return( sem_init( sp, pshared, value ) );
}

static int
sema_wait( sema_t *sp ) {
	return( sem_wait( sp ) );
}

static int
sema_post( sema_t *sp ) {
	return( sem_post( sp ) );
}

static int
sema_destroy( sema_t *sp ) {
	return( sem_destroy( sp ) );
}
#endif	/* !defined(__sun)	*/

/*
 * barrier_init - initialize a barrier variable (count,pshared).
 *
 * Return zero for success or an errno(2) value on failure.
 *
 */

	int
barrier_init( barrier_t *bp, unsigned count ) {
	int	status;

	if ( count == 0 )
		return( EDOM );

	bp->maxcnt	= count;
	bp->runners	= count;
	bp->head	= NULL;

	status	= pthread_mutex_init( &(bp->wait_lk), NULL );

	return( status );
}

/*
 * barrier_wait - wait at a barrier for everyone to arrive.
 *
 */

	int
barrier_wait( barrier_t *bp ) {
	int	status	= 0;

	/* First: get the lock.						*/

	pthread_mutex_lock( &bp->wait_lk );

	/* Check the pre-conditions, implicit is ( bp != NULL )		*/

	assert( bp->runners >= 1 );
	assert( bp->maxcnt  >= 1 );

	if ( bp->runners == 1 ) {	/* last thread to reach barrier	*/
		if ( bp->maxcnt != 1 ) {
			waitlist_t	*hp;

			/*
			 * Traverse the list, posting to each waiter.
			 */

			for ( hp = bp->head; hp; hp = hp->next )
				sema_post( &hp->sem );

			/* Reset for the next iteration			*/

			bp->head	= NULL;
			bp->runners	= bp->maxcnt;
		}

		/* Begin the next cycle					*/

		pthread_mutex_unlock( &bp->wait_lk );
	} else {
		waitlist_t	waiter;	/* my element in the wait list	*/

		bp->runners--;		/* one less runner		*/

		/*
		 * Initialize our semaphore (for blocking) and add it to
		 * the waiters list.
		 */

		sema_init( &waiter.sem, 0, bp->pshared, NULL );

		waiter.next	= bp->head;
		bp->head	= &waiter;

		/*
		 * Now that we're on the wait list, allow the next
		 * thread in.
		 */

		pthread_mutex_unlock( &bp->wait_lk );

		/* wait for the last thread to arrive.		*/

		while ( ( status = sema_wait( &waiter.sem ) ) == EINTR )
			;

		assert( status == 0 );
		sema_destroy( &waiter.sem );
	}

	return( status );
}

/*
 * barrier_destroy - destroy a barrier variable.
 *
 */

	int
barrier_destroy( barrier_t *bp ) {

	assert( bp->head == NULL );
	assert( bp->runners == bp->maxcnt );

	bp->maxcnt	= 0;
	bp->runners	= 0;
	bp->pshared	= 0;

	return( pthread_mutex_destroy( &bp->wait_lk ) );
}
