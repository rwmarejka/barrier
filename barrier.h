#if !defined(BARRIER_INCLUDED)
#define	BARRIER_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

/* Include Files            */

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#if defined(__sun)
#	include <synch.h>
#	include <thread.h>
#elif defined(_POSIX_SEMAPHORES)
#	include <semaphore.h>
#endif

/* Constants and Macros     */

#if defined(NDEBUG)                 /* asserts must be compiled in.		*/
#	undef	NDEBUG
#endif

#define	_POSIX_PTHREAD_SEMANTICS
#define	PTHREAD_BARRIER_INITIALIZER(n)	{PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,(n),(n),0}

#if defined(sun)
#	define	USYNC_PROCESS	PTHREAD_SCOPE_SYSTEM
#	define	USYNC_THREAD	PTHREAD_SCOPE_PROCESS

#	define	SEM_TYPE        sema_t
#	define	SEM_PRIVATE     USYNC_THREAD
#	define	SEM_SHARED      USYNC_PROCESS
#	define	SEM_INIT(s,p)	sema_init((s),0,(p),NULL)
#	define	SEM_DESTROY(s)	sema_destroy(s)
#	define	SEM_WAIT(s)     sema_wait(s)
#	define	SEM_POST(s)     sema_post(s)
#else
#	define	SEM_TYPE        sem_t
#	define	SEM_DESTROY(s)
#	define	SEM_WAIT(s)     sem_wait(s)
#	define	SEM_POST(s)     sem_post(s)
#endif

/* Data Definitions         */

#if   (BARRIER_VERSION == 1)
#elif (BARRIER_VERSION == 3)
#elif (BARRIER_VERSION == 6)
#elif (BARRIER_VERSION == 7)

typedef struct _list {
	SEM_TYPE        sem;
	struct _list	*next;
} waitlist_t;

#elif (BARRIER_VERSION == 8)
#endif

typedef struct {
    unsigned        limit;          /* maximum number of waiters        */
    unsigned        runners;        /* current number of runners        */
    pthead_mutex_t  lk;             /* lock to protect it all           */
#if (BARRIER_VERSION == 1)
    struct _sb {
        pthread_cond_t  cv;         /* cv for waiters at barrier        */
        pthread_mutex_t lk;         /* mutex for waiters at barrier     */
        int             runners;    /* number of running threads        */
    } sb[2];
    struct _sb      *sbp;           /* current sub-barrier              */
#elif (BARRIER_VERSION == 3)
    pthread_cond_t	cv;             /* rendezvous point                 */
	unsigned        generation;     /* barrier usage generation         */
#elif (BARRIER_VERSION == 6)
	SEM_TYPE        **waiters;      /* array of waiters                 */
#elif (BARRIER_VERSION == 7)
	int             pshared;        /* visibility|scope                 */
	waitlist_t      *head;          /* head of wait list                */
#elif (BARRIER_VERSION == 8)
    SEM_TYPE        waiters[];      /* array of waiters                 */
#endif
} barrier_t;

/* External References		*/

extern	int	barrier_init( barrier_t *, unsigned );
extern	int	barrier_wait( barrier_t * );
extern	int	barrier_destroy( barrier_t * );

#if defined(__cplusplus)
}
#endif

#endif	/* BARRIER_INCLUDED	*/
