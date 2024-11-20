# barrier
A Barrier Synchronization Object (in C)

The source includes 3 implementations: generation number, sub-barrier and semaphore. A C pre-processor directive is used to select an implementation at compile-time.

The semaphore barrier is an implementation of US Patent 7,512,950.
The sub-barrier was written in 1995 while I was at Sun Microsystems.
