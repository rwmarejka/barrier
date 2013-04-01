/*
 * B A R R I E R
 *
 * The semaphore version is covered by US patent 7,512,950
 */

/* Feature Test Macros      */
/* Include Files            */

#include <barrier.h>
#include <errno.h>
#include <stdio.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>

/* Constants & Macros       */
/* Data Declarations        */
/* External Declarations	*/

/*
 * barrier_init - initialize a barrier variable.
 *
 * Return zero for success or an errno(2) value on failure.
 */

	int
barrier_init( barrier_t *bp, unsigned count ) {
	int	status  = 0;

    if ( count < 1 )
        return EINVAL;

    bp->limit   = count;
	bp->runners = count;

    if ( status = pthread_mutex_init( &bp->lk, NULL ) )
        return status;

#if (BARRIER_VERSION == BARRIER_SUBBARRIER)

    {
        int i;

        bp->sbp = &bp->sb[0];

        for ( i=0; i < 2; ++i ) {
            struct _sb  *sbp    = &( bp->sb[i] );

            sbp->runners    = count;

            if ( status = pthread_mutex_init( &sbp->lk, NULL ) )
                return status;

            if ( status = pthread_cond_init( &sbp->cv, NULL ) )
                return status;
        }
    }
    
#elif (BARRIER_VERSION == BARRIER_GENERATION)

	bp->generation	= 0;
	status          = pthread_cond_init( &bp->cv, NULL );

#elif (BARRIER_VERSION == BARRIER_SEMAPHORE)
    
    if ( bp->waiters = (sem_t **) calloc( count - 1, sizeof( sem_t * ) ) ) {
        static pthread_mutex_t  sem_lk      = PTHREAD_MUTEX_INITIALIZER;
        static unsigned         sem_count   = 0;

        unsigned    pid = getpid();
        unsigned    i;

        pthread_mutex_lock( &sem_lk );

        for ( i=0; i < ( count - 1 ); ++i ) {
            char    buf[MAXPATHLEN];

            snprintf( buf, sizeof( buf ), "/barrier-%06u-%04u", pid, ++sem_count );

            if ( ( bp->waiters[i] = sem_open( buf, O_CREAT | O_EXCL, 0644, 0 ) ) == SEM_FAILED ) {
                status  = errno;
                break;
            }
        }

        pthread_mutex_unlock( &sem_lk );

        if ( status )
            free( bp->waiters );
	} else
		status	= ENOMEM;
    
#endif

	return( status );
}

/*
 * barrier_wait - wait at a barrier for everyone to arrive.
 *
 */

	int
barrier_wait( barrier_t *bp ) {
	int	status	= 0;

#if (BARRIER_VERSION == BARRIER_SUBBARRIER)

    struct _sb  *sbp    = bp->sbp;          /* which sub-barrier?                           */

    pthread_mutex_lock( &sbp->lk );         /* acquire the sub-barrier lock                 */

    if ( sbp->runners == 1 ) {              /* last thread to reach barrier                 */
        sbp->runners    = bp->limit;        /* reset runner count and switch sub-barriers   */
        bp->sbp         = ( bp->sbp == &bp->sb[0] )? &bp->sb[1] : &bp->sb[0];

        pthread_cond_broadcast( &sbp->cv ); /* wake up the waiters                          */
    } else {
        int     cs_prev;
		int     cs_ignore;
        
        sbp->runners--;                     /* one less runner                              */

        pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_prev );

        while ( sbp->runners != bp->limit )
            pthread_cond_wait( &sbp->cv, &sbp->lk );

        pthread_setcancelstate( cs_prev, &cs_ignore );
    }

    pthread_mutex_unlock( &sbp->lk );       /* release the sub-barrier lock                 */

#elif (BARRIER_VERSION == BARRIER_GENERATION)

	pthread_mutex_lock( &bp->lk );          /* acquire the lock                             */

	if ( bp->runners == 1 ) {               /* last thread to reach barrier                 */
		bp->generation++;                   /* crank generation number                      */
		bp->runners	= bp->limit;            /* reset number of running threads              */

		pthread_cond_broadcast( &bp->cv );
	} else {
		int         cs_prev;
		int         cs_ignore;
		unsigned	genx	= bp->generation;

		bp->runners--;                      /* one less running thread                      */

                                            /* wait until the generation number changes     */
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_prev );

		while ( bp->generation == genx )
			pthread_cond_wait( &bp->cv, &bp->lk );

		pthread_setcancelstate( cs_prev, &cs_ignore );
	}

	pthread_mutex_unlock( &bp->lk );        /* release the lock                             */

#elif (BARRIER_VERSION == BARRIER_SEMAPHORE)

	pthread_mutex_lock( &bp->lk );          /* acquire the lock                             */

	if ( bp->runners == 1 ) {               /* last thread to reach barrier                 */
		unsigned    i;
		unsigned	limit	= bp->limit - 1;

        bp->runners	= bp->limit;            /* reset the number of running threads          */
		pthread_mutex_unlock( &bp->lk );    /* release the lock                             */

        /*
         * The waiters are awoken by a post. Note the order of post and wait allocation
         * are the same: 0 to bp->limit - 1. The post can be done outside of the lock
         * because the order is the same. Should a waiter re-enter, it will be acquire
         * a "free" semaphore.
         */

		for ( i=0; i < limit; ++i )
			sem_post( bp->waiters[i] );
	} else {
		int     cs_prev;
		int     cs_ignore;
                                            /* allocate a semaphore for the waiting thread  */
        sem_t   *wp = bp->waiters[ bp->limit - bp->runners-- ];

		pthread_mutex_unlock( &bp->lk );    /* release the lock                             */

                                            /* wait for the post                            */
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &cs_prev );

		while ( sem_wait( wp ) == EINTR )
			;   /* LINTED - empty loop	*/

		pthread_setcancelstate( cs_prev, &cs_ignore );
	}

#endif

	return( status );
}

/*
 * barrier_destroy - destroy a barrier variable.
 *
 */

	int
barrier_destroy( barrier_t *bp ) {
    int status  = 0;

    pthread_mutex_lock( &bp->lk );          /* acquire the lock                             */

#if (BARRIER_VERSION == BARRIER_SUBBARRIER)

    bp->sb[0].runners   = 0;                /* release the sub-barriers                     */
    bp->sb[1].runners   = 0;
    bp->sbp             = NULL;

    pthread_cond_destroy( &bp->sb[0].cv );
    pthread_cond_destroy( &bp->sb[1].cv );

    pthread_mutex_destroy( &bp->sb[0].lk );
    pthread_mutex_destroy( &bp->sb[1].lk );

#elif (BARRIER_VERSION == BARRIER_GENERATION)

	bp->generation	= 0;
	pthread_cond_destroy( &bp->cv );

#elif (BARRIER_VERSION == BARRIER_SEMAPHORE)
                                            /* release all of the semaphores                */
	{
		unsigned	i;
		unsigned	limit	= bp->limit - 1;

        for ( i=0; i < limit; ++i )
            sem_close( bp->waiters[i] );

		free( bp->waiters );
        bp->waiters	= NULL;
	}

#endif

    bp->limit	= 0;
	bp->runners	= 0;

    pthread_mutex_unlock( &bp->lk );        /* release the lock                             */
    pthread_mutex_destroy( &bp->lk );       /* destroy the lock                             */

    return status;
}
