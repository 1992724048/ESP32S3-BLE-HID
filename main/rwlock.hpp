#pragma once
#include <pthread.h>

class RWLock {
public:
    RWLock() {
        pthread_rwlock_init(&rw_, nullptr);
    }
    ~RWLock() {
        pthread_rwlock_destroy(&rw_);
    }

    void rd_lock() {
        pthread_rwlock_rdlock(&rw_);
    }
    void wr_lock() {
        pthread_rwlock_wrlock(&rw_);
    }
    void unlock() {
        pthread_rwlock_unlock(&rw_);
    }

    struct ReadLock {
        explicit ReadLock(RWLock& l) : lock_(l) {
            lock_.rd_lock();
        }
        ~ReadLock() {
            lock_.unlock();
        }

    private:
        RWLock& lock_;
    };

    struct WriteLock {
        explicit WriteLock(RWLock& l) : lock_(l) {
            lock_.wr_lock();
        }
        ~WriteLock() {
            lock_.unlock();
        }

    private:
        RWLock& lock_;
    };

private:
    pthread_rwlock_t rw_;
};
