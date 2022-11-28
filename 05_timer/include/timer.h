#ifndef _TIMER_H_
#define _TIMER_H_

#include <map>
#include <set>

#include <pthread.h>
#include <time.h>

typedef void (*TimerCallback)(void *);
class TimerCallbackEntity;

/**
 * @brief The top level "Timer" class. It provides an interface of adding or
 * removing callbacks.
 */
class Timer {
  public:
    virtual bool AddCallback(const TimerCallbackEntity &cbe) = 0;
    virtual bool RemoveCallback(const TimerCallback cb) = 0;
};

/**
 * @brief The callback entity. It contains information of callback function
 * pointer, callback function argument and the deadline at which callback
 * function should be called.
 */
class TimerCallbackEntity {
  public:
    TimerCallbackEntity() = default;
    TimerCallbackEntity(const TimerCallbackEntity &other) = default;
    TimerCallbackEntity(const TimerCallback cb, void *data,
                        const long interval_ms, const long interval_ns = 0);

    void Reset();
    bool IsValid() const;
    bool operator<(const TimerCallbackEntity &other) const;

    const long GetIntervalMs() const;
    const long GetIntervalNs() const;
    const struct timespec *GetDeadline() const;
    TimerCallback GetCallback() const;
    void *GetData() const;

  private:
    TimerCallback callback_;
    void *data_;

    long interval_ms_;
    long interval_ns_;
    struct timespec deadline_;
};

/**
 * @brief Timer implementation base on pthread_cond_timedwait.
 */
class MxTimer final : public Timer {
  public:
    MxTimer();
    virtual ~MxTimer();

    bool AddCallback(const TimerCallbackEntity &cbe) override;
    bool RemoveCallback(const TimerCallback cb) override;

  private:
    TimerCallbackEntity GetFirstCallbackEntity() const;
    void HandleCallbacks();

    static void *WorkerThreadEntry(void *data);

  private:
    pthread_t worker_thread_;

    std::set<TimerCallbackEntity> cbe_set_;
    std::map<TimerCallback, TimerCallbackEntity> cbe_map_;

    pthread_mutex_t cbe_lock_;
    pthread_cond_t cbe_changed_cond_;
    bool cbe_changed_;
};

#endif