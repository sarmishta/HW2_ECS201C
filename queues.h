#ifndef QUEUES_H_
#define QUEUES_H_

#include <atomic>
#include <cassert>
#include <deque>
#include <mutex>
#include <iostream>

// Defines the interface for the Fixed Size Queue
template<typename T>
class FixedSizeQueueInterface {
 public:
  virtual ~FixedSizeQueueInterface() { }
  virtual bool Read(T* data) = 0;
  virtual bool Write(const T& data) = 0;
};

// Implements a fixed-size queue using a Mutex
/* Update this to use reader writer locks */
template<typename T>
class MutexFixedSizeQueue : public FixedSizeQueueInterface<T> {
 public:

  // Simple helper class to ensure a lock is unlocked once the scope is exited
  class ScopedLock {
   public:
    ScopedLock(std::mutex* mutex) : mutex_(mutex) {
      mutex_->lock();
    }
    ~ScopedLock() {
      mutex_->unlock();
    }
   private:
    std::mutex* mutex_;
  };

  MutexFixedSizeQueue(int max_size) : max_size_(max_size), buffer_(new Entry[max_size])
  { }

  // Reads the next data item into 'data', returns true
  // if successful or false if the queue was empty.
  bool Read(T* data) {
    ScopedLock l(&mutex_);
    uint32_t location = tail_ % max_size_;
    if (!buffer_[location].valid || tail_ == head_) {
      return false; // empty
    }
    assert(head_ >= tail_);
    assert(buffer_[location].valid == true);
    *data = buffer_[location].data;
    buffer_[location].valid = false;
    tail_ = tail_ + 1;
    return true;
  }

  // Writes 'data' into the queue.  Returns true if successful
  // or false if the queue was full.
  bool Write(const T& data) {
    ScopedLock l(&mutex_);
    uint32_t location = head_ % max_size_;
    if (buffer_[location].valid || head_ == tail_ + max_size_) {
      return false; // full
    }
    assert(head_ >= tail_);
    assert(buffer_[location].valid == false);
    buffer_[location].data = data;
    buffer_[location].valid = true;
    head_ = head_ + 1;
    return true;
  }

  bool isEmpty() {
    return head_ == tail_ && !buffer_[tail_ % max_size_].valid;
  }

 private:struct Entry {
    T data;
    bool valid = false;
  };

  int max_size_;
  Entry* buffer_;

  uint32_t head_ __attribute__((aligned(64))) = 0;
  uint32_t tail_ __attribute__((aligned(64))) = 0;

  std::mutex mutex_;
};


// Implements a fixed-size queue using no lock, but limited to a single
// producer thread and a single consumer thread.
template<typename T>
class SingleProducerSingleConsumerFixedSizeQueue : public FixedSizeQueueInterface<T> {
 public:
  SingleProducerSingleConsumerFixedSizeQueue(int max_size)
    : max_size_(max_size),
      buffer_(new Entry[max_size]),
      head_(0),
      tail_(0) {
  }

  ~SingleProducerSingleConsumerFixedSizeQueue() {
    delete[] buffer_;
  }

  virtual bool Read(T* data) {
    /* Needs to be updated */
    // head_=head_.load ();
    // tail_=tail_.load ();

    uint32_t  location= tail_ % max_size_;
    if (!buffer_[location].valid || tail_ == head_ ) {
      return false; // empty
    }
    assert(head_ >= tail_);
    assert(buffer_[location].valid == true);
    *data = buffer_[location].data;
    buffer_[location].valid.store(false);
    tail_.store (tail_+1);
    return true;
  }

  virtual bool Write(const T& data) {
    /* Needs to be updated */
    // head_ = head_.load ();
    // tail_ = tail_.load ();

    uint32_t  location= head_ % max_size_;

    if (buffer_[location].valid || head_ == tail_ + max_size_) {
      return false; // full
    }
    assert(head_ >= tail_);
    assert(buffer_[location].valid == false);
    buffer_[location].data = data;
     
    buffer_[location].valid.store(true);
    //buffer_[location].valid = true;
    head_.store (head_+1);
   
    return true;
  }

  bool isEmpty() {
   return head_ == tail_ && !buffer_[tail_ % max_size_].valid;
  }

 private:
  struct Entry {
    T data;
    std::atomic<bool> valid;
  };

  int max_size_;
  Entry* buffer_;
  std::atomic<int> head_ __attribute__((aligned(64)));
  std::atomic<int> tail_ __attribute__((aligned(64)));
};


// Implements a fixed-size queue using no lock, but limited to a single
// producer thread and multiple consumer threads.
template<typename T>
class SingleProducerMultipleConsumerFixedSizeQueue : public FixedSizeQueueInterface<T> {
 public:
  SingleProducerMultipleConsumerFixedSizeQueue(int max_size)
    : max_size_(max_size),
      buffer_(new Entry[max_size]),
      head_(0),
      tail_(0) {
    /* Needs to be updated */
    for (int i = 0; i < max_size; ++i) {
     buffer_[i].valid = false;
     }
  }

  ~SingleProducerMultipleConsumerFixedSizeQueue() {
    delete[] buffer_;
  }
  virtual bool Read(T* data) {

    int tail = this->tail_.load();
    uint32_t location = tail % this->max_size_;  
    if (!this->buffer_[location].valid|| this->tail_ == this->head_) {
      return false;//empty
    } 
    if (!this->tail_.compare_exchange_strong(tail, tail + 1)) {
      return false;
    } 
    assert(this->buffer_[location].valid == true);
    assert(this->head_ >= tail);
    *data = this->buffer_[location].data;
    this->buffer_[location].valid = false;
   return true;
  }
  virtual bool Write(const T& data) {
    uint32_t  location = this->head_ % this->max_size_;
    if (this->buffer_[location].valid || this->head_ == this->tail_ + this->max_size_) {
      return false;//full
    }


    
    assert(this->head_ >= this->tail_);
    assert(this->buffer_[location].valid == false);
    this->buffer_[location].data = data;
    this->buffer_[location].valid = true;
     this-> head_.store (this->head_+1);
    return true;
  }

  bool isEmpty() {
    return this->head_ == this->tail_ && this->buffer_[this->tail_ % this->max_size_].valid == false;
  }

 private:
  struct Entry {
    T data;
    bool valid = false;
  };

  int max_size_;
  Entry* buffer_;
  //std::atomic<bool> is_reading;
  std::atomic<int> head_ __attribute__((aligned(64)));
  std::atomic<int> tail_ __attribute__((aligned(64)));
};


// Implements a fixed-size queue using no lock, but limited to multiple
// producer threads and a single consumer thread.
template<typename T>
class MultipleProducerSingleConsumerFixedSizeQueue : public FixedSizeQueueInterface<T> {
 public:
  MultipleProducerSingleConsumerFixedSizeQueue(int max_size)
    : max_size_(max_size),
      buffer_(new Entry[max_size]),
      head_(0),
      tail_(0) {
    /* Needs to be updated */
    for (int i = 0; i < max_size; ++i) {
      buffer_[i].valid = false;
    }
  }

  ~MultipleProducerSingleConsumerFixedSizeQueue() {
    delete[] buffer_;
  }

 virtual bool Read(T* data) {
    uint32_t location = tail_ % this->max_size_;  
    if (!this->buffer_[location].valid|| this->tail_ == this->head_) {
      return false;//empty
    } 

    assert(this->buffer_[location].valid == true);
    assert(this->head_ >= tail_);
    *data = this->buffer_[location].data;
    this->buffer_[location].valid = false;
    this-> tail_.store (this->tail_+1);
   return true;
  }
  virtual bool Write(const T& data) {
   int head = this->head_.load();
    uint32_t location = head % this->max_size_;  
    if (this->buffer_[location].valid || this->head_ == this->tail_ + this->max_size_) {
      return false;//full
    }

 if (!this->head_.compare_exchange_strong(head, head + 1)) {
      return false;
    } 
    
    assert(head>= this->tail_);
    assert(this->buffer_[location].valid == false);
    this->buffer_[location].data = data;
    this->buffer_[location].valid = true;
    // this-> head_.store (this->head_+1);
    return true;
  }
  bool isEmpty() {
    return this->head_ == this->tail_ && this->buffer_[this->tail_ % this->max_size_].valid == false;
  }

 private:
  struct Entry {
    T data;
    bool valid;
  };

  int max_size_;
  Entry* buffer_;
  std::atomic<int> head_ __attribute__((aligned(64)));
  std::atomic<int> tail_ __attribute__((aligned(64)));
};


#endif  // QUEUES_H_
