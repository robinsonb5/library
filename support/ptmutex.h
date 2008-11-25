/* Wrapper around pthread mutices */

#ifndef PTMUTEX_H
#define PTMUTEX_H

#include <glib.h>

// #if defined HAVE_LIBPTHREAD || defined HAVE_LIBPTHREADGC2

#include <pthread.h>

class PTMutex
{
	public:
	PTMutex();
	~PTMutex();
	void ObtainMutex();
	bool AttemptMutex();
	void ReleaseMutex();
	protected:
	pthread_mutex_t mutex;
	friend class Thread;
};

//#else
#if 0

// We provide a dummy mutex implemntation for those occasions when no thread
// implementation is available.  This allows such things as the profilemanager
// to be mutex-protected against concurrent access, yet still be usable without
// threads (in which case concurrent access is hardly likely to be problem!

class PTMutex
{
	public:
	PTMutex();
	~PTMutex();
	void ObtainMutex();
	bool AttemptMutex();
	void ReleaseMutex();
	protected:
	friend class Thread;
};

#endif

#endif

