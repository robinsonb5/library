#include <iostream>

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rwmutex.h"

using namespace std;

// #if defined HAVE_LIBPTHREAD || defined HAVE_LIBPTHREADGC2

// pthreads-based implementation

RWMutex::RWMutex() : PTMutex(), lockcount(0), exclusive(0)
{
	for(int i=0;i<RWMUTEX_THREADS_MAX;++i)
	{
		counttable[i].id=0;
		counttable[i].count=0;
	}
	pthread_cond_init(&cond,0);
}


RWMutex::~RWMutex()
{
}


// ObtainMutex - obtains the mutex in exclusive mode.
// Succeeds if there are no locks currently held, or if all currently held
// locks are held by the current thread.

// Once the lock is held, no other threads will be able to obtain a lock,
// either exclusive or shared.  The thread holding the lock can, however,
// obtain further locks of either exclusive or shared type.
// This differs from pthreads rwlocks - their behaviour is undefined if a
// thread holding a write-lock attempts to obtain a read-lock or vice versa.

// If a thread holds a shared lock, then obtains an exclusive lock, and
// another shared lock, the lock will remain in exclusive mode until the
// inner two locks have been released, then revert to shared mode.

void RWMutex::ObtainMutex()
{
//	static int id=0;
//	int tid=id++;
	PTMutex::ObtainMutex();
//	cerr << tid << ": Obtaining exclusive" << endl;
	while(!CheckExclusive())
	{
//		cerr << tid << ": Obtain exclusive failed - waiting..." << endl;
//		cerr << "Exclusive count: " << exclusive << endl;
		pthread_cond_wait(&cond,&mutex);
	}
//	cerr << tid << ": Obtain exclusive succeeded - incrementing..." << endl;
	Increment();
	if(exclusive==0)
		++exclusive;
//	Dump();
	PTMutex::ReleaseMutex();
}


// ObtainMutexShared - obtains the mutex in non-exclusive mode.
// Succeeds if there are no exclusive locks currently held

// Once the lock is held, other threads will be able to obtain shared locks,
// but not exclusive locks.  The thread holding the lock can, however, obtain
// a subsequent exclusive lock.  The lock will revert to shared mode when
// the second lock is released.
// This differs from pthreads rwlocks - their behaviour is undefined if a
// thread holding a write-lock attempts to obtain a read-lock or vice versa.

void RWMutex::ObtainMutexShared()
{
//	static int id=0;
//	int tid=id++;
	PTMutex::ObtainMutex();
//	cerr << tid << ": Obtaining shared" << endl;
	while((exclusive!=0) && !CheckExclusive())
	{
//		cerr << tid << ": Obtain shared failed - waiting..." << endl;
//		Dump();

		// We must wait until the exclusive flag is clear;
		pthread_cond_wait(&cond,&mutex);
	}
//	cerr << tid << ": Obtain shared succeeded - incrementing..." << endl;
	Increment();
//	Dump();
	PTMutex::ReleaseMutex();
}


bool RWMutex::AttemptMutex()
{
	bool result=false;
	PTMutex::ObtainMutex();
	if(CheckExclusive())
	{
//		cerr << "Obtained exclusive lock - lock count: " << lockcount << endl;
		Increment();
		if(exclusive==0)
			++exclusive;
//		Dump();
		result=true;
	}
	PTMutex::ReleaseMutex();
	return(result);
}


bool RWMutex::AttemptMutexShared()
{
	bool result=false;
	PTMutex::ObtainMutex();
	if((exclusive==0) || (CheckExclusive()))
	{
		Increment();
//		Dump();
		result=true;
	}
	PTMutex::ReleaseMutex();
	return(result);
}


void RWMutex::ReleaseMutex()
{
	PTMutex::ObtainMutex();
	Decrement();
//	Dump();
	pthread_cond_broadcast(&cond);
	PTMutex::ReleaseMutex();
}


bool RWMutex::CheckExclusive()
{
	if(lockcount==0)
		return(true);
//	if(exclusive==0);
//		return(true);
	// If the lockcount is greater than zero, then we have to
	// step through the thread table making sure that the
	// current thread is the only one holding locks.
#ifdef WIN32
	void *current=pthread_self().p;
#else
	pthread_t current=pthread_self();
#endif
//	cerr << "Current thread: " << current << endl;
	for(int i=0;i<RWMUTEX_THREADS_MAX;++i)
	{
		// If any thread other than this one has count>0 return false;
		if(counttable[i].id!=current && counttable[i].count!=0)
			return(false);
		// If the current thread's count == the global lockcount, return true.
		if(counttable[i].id==current && counttable[i].count==lockcount)
			return(true);
	}
	// If we reached here, then the global lockcount is greater than zero, but
	// the thread table is inconsistent.  Succeed grudgingly.
	cerr << "RWMutex: inconsistent locking data." << endl;
	return(true);
}


void RWMutex::Increment()
{
	++lockcount;
	if(exclusive)
		++exclusive;
	// Find the current thread in the table and increment its count
#ifdef WIN32
	void *current=pthread_self().p;
#else
	pthread_t current=pthread_self();
#endif
//	cerr << "Increment - searching for thread: " << current << endl;
	for(int i=0;i<RWMUTEX_THREADS_MAX;++i)
	{
		if(counttable[i].id==current)
		{
//			cerr << "found" << endl;
			++counttable[i].count;
			return;
		}
	}
	// If none found, add a new entry, and set the count to 1;
//	cerr << "Increment - searching for free slot" << endl;
	while(1)
	{
		for(int i=0;i<RWMUTEX_THREADS_MAX;++i)
		{
			if(counttable[i].id==0)
			{
//				cerr << "found" << endl;
				counttable[i].id=current;
				counttable[i].count=1;
				return;
			}
		}
		// If there were no free slots, complain.
		cerr << "RWMutex: thread table full - waiting..." << endl;
		pthread_cond_wait(&cond,&mutex);
		cerr << "RWMutex - trying again to find a free slot... " << endl;
	}
}


void RWMutex::Decrement()
{
	--lockcount;
	if(exclusive)
		--exclusive;
#ifdef WIN32
	void *current=pthread_self().p;
#else
	pthread_t current=pthread_self();
#endif
//	cerr << "Decrement - searching for thread: " << current << endl;
	for(int i=0;i<RWMUTEX_THREADS_MAX;++i)
	{
		if(counttable[i].id==current)
		{
			--counttable[i].count;
			if(counttable[i].count==0)
				counttable[i].id=0;
			return;
		}
	}
	// If thread was not found, complain.
//	cerr << "RWMutex: thread not found in table." << endl;
}


void RWMutex::Dump()
{
	cerr << "Locks held: " << lockcount << endl;
	cerr << "Exclusive count: " << exclusive << endl;
	for(int i=0;i<RWMUTEX_THREADS_MAX;++i)
	{
		if(counttable[i].id)
			cerr << "Thread: " << counttable[i].id << ", count: " << counttable[i].count << endl;
	}
}

// #else
#if 0
// Dummy implementation.  Obtaining the rwlock always succeeds.

RWMutex::RWMutex()
{
	cerr << "Warning - building a dummy rwlock" << endl;
}


RWMutex::~RWMutex()
{
}


void RWMutex::ObtainMutex()
{
	cerr << "Warning - obtaining a dummy rwlock" << endl;
}


bool RWMutex::AttemptMutex()
{
	cerr << "Warning - attempting a dummy rwlock" << endl;
	return(true);
}


void RWMutex::ReleaseMutex()
{
	cerr << "Warning - releasing a dummy rwlock" << endl;
}


#endif

