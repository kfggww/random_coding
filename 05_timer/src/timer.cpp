#include "timer.h"

/**
 * @brief if the absolute time "now" is later than the "deadline".
 *
 * @param now
 * @param deadline
 *
 * @return true if "now" is later than "deadline"
 */
static bool LaterThan(const struct timespec *now,
                      const struct timespec *deadline) {
    if (nullptr == now || nullptr == deadline)
        return false;

    if (now->tv_sec > deadline->tv_sec)
        return true;
    else if (now->tv_sec == deadline->tv_sec &&
             now->tv_nsec >= deadline->tv_nsec)
        return true;
    else
        return false;
}

/**
 * @brief Construct callback entity. The callback should be called after
 * interval_ms milliseconds or interval_ns nanoseconds.
 *
 * @param cb: callback function pointer
 * @param data: callback function argument
 * @param interval_ms: time interval measured in milliseconds
 * @param interval_ns: timer interval measured in nanoseconds
 *
 * @note NEVER set both the interval_ms and interval_ns to non-zero value
 */
TimerCallbackEntity::TimerCallbackEntity(const TimerCallback cb, void *data,
                                         const long interval_ms,
                                         const long interval_ns)
    : callback_(cb), data_(data), interval_ms_(interval_ms),
      interval_ns_(interval_ns) {
    clock_gettime(CLOCK_MONOTONIC, &deadline_);
    if (interval_ms != 0) {
        deadline_.tv_sec += interval_ms / 1000;
        deadline_.tv_nsec += (interval_ms % 1000) * 1000000;
    } else if (interval_ns != 0) {
        deadline_.tv_sec += interval_ns / 1000000000;
        deadline_.tv_nsec += (interval_ns % 1000000000);
    }
}

/**
 * @brief Reset the callback entity to invalid state.
 */
void TimerCallbackEntity::Reset() {
    interval_ms_ = 0;
    interval_ns_ = 0;
    callback_ = nullptr;
    data_ = nullptr;
}

/**
 * @brief If the callback entity is valid.
 *
 * @return true if valid
 */
bool TimerCallbackEntity::IsValid() const {
    if (interval_ms_ < 0 || interval_ns_ < 0 || callback_ == nullptr ||
        (interval_ms_ == 0 && interval_ns_ == 0))
        return false;
    return true;
}

/**
 * @brief If the deadline point is smaller than the other's.
 */
bool TimerCallbackEntity::operator<(const TimerCallbackEntity &other) const {
    return !LaterThan(&deadline_, &other.deadline_);
}

/**
 * @brief Get callback function interval_ms.
 */
const long TimerCallbackEntity::GetIntervalMs() const { return interval_ms_; }

/**
 * @brief Get callback function interval_ns.
 */
const long TimerCallbackEntity::GetIntervalNs() const { return interval_ns_; }

/**
 * @brief Get callback function deadline.
 */
const struct timespec *TimerCallbackEntity::GetDeadline() const {
    return &deadline_;
}

/**
 * @brief Get callback function pointer.
 */
TimerCallback TimerCallbackEntity::GetCallback() const { return callback_; }

/**
 * @brief Get callback function argument.
 */
void *TimerCallbackEntity::GetData() const { return data_; }

/**
 * @brief Construct timer. Worker thread is started.
 */
MxTimer::MxTimer() : cbe_changed_(false) {
    pthread_mutex_init(&cbe_lock_, NULL);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&cbe_changed_cond_, &cond_attr);

    pthread_create(&worker_thread_, NULL, MxTimer::WorkerThreadEntry, this);
}

/**
 * @brief Destruct timer. Woker thread is cancled.
 */
MxTimer::~MxTimer() { pthread_cancel(worker_thread_); }

/**
 * @brief Add callback entity to the timer.
 *
 * @param cbe: callback entity
 *
 * @return true if success
 */
bool MxTimer::AddCallback(const TimerCallbackEntity &cbe) {
    if (!cbe.IsValid())
        return false;

    pthread_mutex_lock(&cbe_lock_);
    TimerCallback cb = cbe.GetCallback();
    if (cbe_map_.find(cb) != cbe_map_.end()) {
        pthread_mutex_unlock(&cbe_lock_);
        return false;
    }

    cbe_map_[cb] = cbe;
    cbe_set_.insert(cbe);

    cbe_changed_ = true;
    pthread_mutex_unlock(&cbe_lock_);
    pthread_cond_signal(&cbe_changed_cond_);

    return true;
}

/**
 * @brief Remove callback from timer.
 *
 * @param cb: callback function pointer that has been added before
 *
 * @return true if success
 */
bool MxTimer::RemoveCallback(const TimerCallback cb) {

    if (nullptr == cb)
        return false;

    pthread_mutex_lock(&cbe_lock_);
    if (cbe_map_.find(cb) == cbe_map_.end()) {
        pthread_mutex_unlock(&cbe_lock_);
        return false;
    }

    TimerCallbackEntity cbe = cbe_map_[cb];
    cbe_set_.erase(cbe);
    cbe_map_.erase(cb);

    cbe_changed_ = true;
    pthread_mutex_unlock(&cbe_lock_);
    pthread_cond_signal(&cbe_changed_cond_);

    return true;
}

/**
 * @brief Get the first callback entity lies on time axis.
 *
 * @return The callback entity with earliest deadline
 */
TimerCallbackEntity MxTimer::GetFirstCallbackEntity() const {
    TimerCallbackEntity cbe(nullptr, nullptr, 0);
    if (cbe_set_.empty())
        return cbe;

    cbe = *cbe_set_.begin();
    return cbe;
}

/**
 * @brief Handle all the expired callbacks.
 */
void MxTimer::HandleCallbacks() {
    struct timespec now;
    auto itr_cbe = cbe_set_.begin();
    while (itr_cbe != cbe_set_.end()) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (LaterThan(&now, itr_cbe->GetDeadline())) {
            TimerCallback cb = itr_cbe->GetCallback();
            void *data = itr_cbe->GetData();
            cb(data);
            cbe_map_.erase(cb);
            itr_cbe = cbe_set_.erase(itr_cbe);
        } else
            break;
    }
}

/**
 * @brief The worker thread entry of low resolution timer.
 *
 * @param data: pointer to a low resolution timer
 *
 * @return NULL
 */
void *MxTimer::WorkerThreadEntry(void *data) {
    MxTimer *timer = static_cast<MxTimer *>(data);
    if (nullptr == timer)
        return NULL;

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int rc = 0;
    while (true) {
        pthread_mutex_lock(&timer->cbe_lock_);

        struct timespec tv;
        TimerCallbackEntity cbe = timer->GetFirstCallbackEntity();
        if (cbe.IsValid())
            tv = *cbe.GetDeadline();
        else {
            clock_gettime(CLOCK_MONOTONIC, &tv);
            tv.tv_sec += 2;
        }

        while (!timer->cbe_changed_ && 0 == rc) {
            rc = pthread_cond_timedwait(&timer->cbe_changed_cond_,
                                        &timer->cbe_lock_, &tv);
        }

        if (timer->cbe_changed_) {
            timer->cbe_changed_ = false;
            pthread_mutex_unlock(&timer->cbe_lock_);
            continue;
        }

        timer->HandleCallbacks();

        pthread_mutex_unlock(&timer->cbe_lock_);
    }

    return NULL;
}
