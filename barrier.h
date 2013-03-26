#if !defined(BARRIER_INCLUDED)
#define	BARRIER_INCLUDED

#pragma ident "@(#) barrier.h 1.2 02/06/10 SMI"

#if defined(__cplusplus)
extern "C" {
#endif

#if BARRIER_VERSION == 1

#include <pthread.h>
#include <errno.h>


typedef struct {
        unsigned    maxcnt;                 /* maximum number of runners    */
        struct _sb {
                pthread_cond_t  wait_cv;        /* cv for waiters at barrier    */
                pthread_mutex_t wait_lk;        /* mutex for waiters at barrier */
                int     runners;        /* number of running threads    */
        } sb[2];
        struct _sb      *sbp;           /* current sub-barrier          */
} barrier_t;

#elif BARRIER_VERSION == 3

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#if defined(sun)
#	include <thread.h>
#else
#	include <semaphore.h>
#endif

#define	PTHREAD_BARRIER_INITIALIZER(n)	{ PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, (n), (n), 0 }

typedef struct {
	pthread_mutex_t	lk;		/* lock to protect barrier object	*/
	pthread_cond_t	cv;		/* rendezvous point			*/
	unsigned	limit;		/* number of threads using barrier	*/
	unsigned	runners;	/* number of non-waiting threads	*/
	unsigned	generation;	/* barrier usage generation		*/
} barrier_t;

#elif BARRIER_VERSION == 6

#define	_POSIX_PTHREAD_SEMANTICS

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

/*
 * On Sun systems (read: Solaris) use the Solaris thread library semaphore implementation.
 * On other systems the POSIX.1b (real-time) semaphore could be used. As for Windows, it would
 * require something different again (Win32 Semaphore object).
 *
 * XXX - note that we require direct access to the "count" member of the semaphore.
 */

#if defined(sun)
#	include <thread.h>
#endif

#if defined(NDEBUG)
#	undef	NDEBUG
#endif

#if defined(sun)
#	define	SEM_TYPE	sema_t
#	define	SEM_PRIVATE	USYNC_THREAD
#	define	SEM_SHARED	USYNC_PROCESS
#	define	SEM_INIT(s,p)	sema_init((s),0,(p),NULL)
#	define	SEM_DESTROY(s)	sema_destroy(s)
#	define	SEM_WAIT(s)	sema_wait(s)
#	define	SEM_POST(s)	sema_post(s)
#else
#	define	SEM_TYPE	sem_t
#	define	SEM_DESTROY(s)	
#	define	SEM_WAIT(s)	sem_wait(s)
#	define	SEM_POST(s)	sem_post(s)
#endif

typedef struct {
	pthread_mutex_t	lk;		/* lock to protect barrier object	*/
	unsigned	limit;		/* number of threads using barrier	*/
	unsigned	runners;	/* number of non-waiting threads	*/
	SEM_TYPE	**waiters;	/* array of waiters			*/
} barrier_t;

#elif BARRIER_VERSION == 7

/* Feature Test Macros		*/

/* Include Files		*/

#include <unistd.h>
#include <errno.h>

#if defined(_POSIX_THREADS)
#	include <pthread.h>
#endif

#if defined(__sun)
#	include <synch.h>
#elif defined(_POSIX_SEMAPHORES)
#	include <semaphore.h>
#else
#	error "Cannot find system semaphore definition."
#endif

#if defined(NDEBUG)		/* asserts must be compiled in.		*/
#	undef	NDEBUG
#endif

#include <assert.h>

/* Constants & Macros		*/

#if !defined(__sun)
#	define	USYNC_PROCESS	PTHREAD_SCOPE_SYSTEM
#	define	USYNC_THREAD	PTHREAD_SCOPE_PROCESS
#endif

/* Data Declarations		*/

#if !defined(__sun)
typedef sem_t	sema_t;
#endif

typedef struct _list {
	sema_t		sem;
	struct _list	*next;
} waitlist_t;

typedef struct {
	unsigned	maxcnt;		/* maximum number of runners	*/
	pthread_mutex_t	wait_lk;	/* mutex for waiters at barrier	*/
	unsigned	runners;	/* number of running threads	*/
	int		pshared;	/* visibility|scope		*/
	waitlist_t	*head;		/* head of wait list		*/
} barrier_t;
#endif

/* External References		*/

extern	int	barrier_init( barrier_t *, unsigned );
extern	int	barrier_wait( barrier_t * );
extern	int	barrier_destroy( barrier_t * );

#if defined(__cplusplus)
}
#endif

#endif	/* BARRIER_INCLUDED	*/
