// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifndef LINKEDBUFFER_H
#define LINKEDBUFFER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "LinkedBuffer_fwd.h"

namespace QuickFAST
{
  namespace Communication
  {
    ///@brief A buffer of unsigned chars to be stored in a linked list.
    class LinkedBuffer
    {
    public:
      /// @brief Construct with a given size
      /// @param capacity is how many bytes to allocate
      LinkedBuffer(size_t capacity)
        : link_(0)
        , buffer_(new unsigned char [capacity])
        , capacity_(capacity)
        , used_(0)
        , extra_(0)
      {
      }

      /// @brief Construct an empty buffer
      ///
      /// This can be used either as a list head in which case it will never contain data
      /// or it can be used to link to external data.
      LinkedBuffer()
        : link_(0)
        , buffer_(0)
        , capacity_(0)
        , used_(0)
        , extra_(0)
      {
      }

      ~LinkedBuffer()
      {
        if(capacity_ != 0)
        {
          delete[] buffer_;
        }
      }

      /// @brief Access the raw buffer
      /// @returns a pointer to the raw buffer
      unsigned char * get()
      {
        return buffer_;
      }

      /// @brief Constant access to the raw buffer
      /// @returns a const pointer to the raw buffer
      const unsigned char * get() const
      {
        return buffer_;
      }

      /// @brief Support indexing
      unsigned char & operator[](size_t index)
      {
        if((capacity_ == 0 && index >= used_) || index >= capacity_)
        {
          throw std::range_error("LinkedBuffer: Index out of bounds.");
        }
        return buffer_[index];
      }

      /// @brief Support indexing constant
      const unsigned char & operator[](size_t index) const
      {
        if(index >= used_)
        {
          throw std::range_error("LinkedBuffer (const): Index out of bounds.");
        }
        return buffer_[index];
      }

      /// @brief Access the allocated size.
      /// @returns the buffer's capacity
      size_t capacity() const
      {
        return capacity_;
      }

      /// @brief set external buffer
      ///
      void setExternal(const unsigned char * externalBuffer, size_t used, void * extra = 0)
      {
        if(capacity_ != 0)
        {
          delete[] buffer_;
          capacity_ = 0;
        }
        buffer_ = const_cast<unsigned char *>(externalBuffer);
        used_ = used;
        extra_ = extra;
      }

      /// @brief Set the number of bytes used in this buffer
      /// @param used byte count
      void setUsed(size_t used)
      {
        used_ = used;
      }

      /// @brief Access the number of bytes used in this buffer
      /// @returns used byte count
      size_t used()const
      {
        return used_;
      }

      /// @brief Linked List support: Set the link
      /// @param link pointer to buffer to be linked <b>after</b> this one.
      void link(LinkedBuffer * link)
      {
        link_ = link;
      }

      /// @brief Linked List support: access the link.
      /// @returns the buffer <b>after</b> this one.  Zero if none
      LinkedBuffer * link() const
      {
        return link_;
      }

      /// @brief capture extra info when used with external data
      void setExtra(void *extra)
      {
        extra_ = extra;
      }

      /// @brief return extra info when used with external data
      void * extra() const
      {
        return extra_;
      }

    private:
      LinkedBuffer * link_;
      unsigned char * buffer_;
      size_t capacity_;
      size_t used_;
      void * extra_;
    };

    /// @brief No frills ordered collection of buffers: FIFO
    ///
    /// No internal synchronization.
    /// This object does not manage buffer lifetimes.  It assumes
    /// that buffers outlive the collection.
    class BufferQueue
    {
    public:
      BufferQueue()
        : head_()
        , tail_(&head_)
      {
      }

      ///@brief return true if empty
      bool isEmpty() const
      {
        return head_.link() == 0;
      }

      /// @brief Push a single buffer onto the queue
      /// @param buffer is the buffer to be pushed
      /// @returns true if this queue was empty before the push; and not empty afterward
      bool push(LinkedBuffer * buffer)
      {
        bool first = isEmpty();
        assert(buffer != 0);
        buffer->link(0);
        tail_->link(buffer);
        tail_ = buffer;
        return first;
      }


      /// @brief Push a single buffer onto the front of the queue
      /// @param buffer is the buffer to be pushed
      /// @returns true if this queue was empty before the push; and not empty afterward
      bool push_front(LinkedBuffer * buffer)
      {
        bool first = isEmpty();
        assert(buffer != 0);
        buffer->link(&head_);
        head_.link(buffer);
        if(first)
        {
          tail_ = buffer;
        }
        return first;
      }

      /// @brief Push a list of buffers onto the queue.
      /// @param buffer is the first of a linked list of buffers to be added to the queue
      /// @returns true if this queue was empty before the push; and not empty afterward
      bool pushList(LinkedBuffer * buffer)
      {
        bool first = isEmpty();
        assert(buffer != 0);
        tail_->link(buffer);
        while(buffer->link() != 0)
        {
          buffer = buffer->link();
        }
        tail_ = buffer;
        return first;
      }

      /// @brief Push buffers from one queue to another.
      ///
      /// The source queue is empty after this call.
      /// @param queue the source queue.
      /// @returns true if this queue was empty before the push; and not empty afterward
      bool push(BufferQueue & queue)
      {
        bool first = isEmpty();
        LinkedBuffer * head = queue.head_.link();
        if(head != 0)
        {
          tail_->link(head);
          tail_ = queue.tail_;
        }
        queue.head_.link(0);
        queue.tail_ = & queue.head_;
        return first && !isEmpty();
      }

      /// @brief Pop the first entry from the queue
      /// @returns the buffer or 0 if the queue is empty.
      LinkedBuffer * pop()
      {
        LinkedBuffer * result = head_.link();
        if(result != 0)
        {
          head_.link(result->link());
          result->link(0);
          if(head_.link() == 0)
          {
            tail_ = &head_;
          }
        }
        return result;
      }

      /// @brief Pop the entire contents of the queue as a linked list.
      LinkedBuffer * popList()
      {
        LinkedBuffer * result = head_.link();
        head_.link(0);
        tail_ = &head_;
        return result;
      }

      /// @brief nondestructive access to the first item in the queue
      LinkedBuffer * begin() const
      {
        return head_.link();
      }

      /// @brief to enable stl-like algorithms
      LinkedBuffer * end() const
      {
        return 0;
      }

    private:
      LinkedBuffer head_;
      LinkedBuffer * tail_;
    };

    ///@brief Keep a queue of buffers waiting to be serviced by one server thread.
    ///
    /// Buffers may be generated by multiple threads, but must be serviced by a single
    /// thread (at any one time.)
    ///
    /// A thread with a buffer that needs to be processed locks a mutex and calls push()
    /// If push returns true that thread is now responsible for servicing the queue.
    /// To service the queue,
    ///    release the mutex (only one thread services at a time
    ///                        so no synchronization is needed)
    ///    for(bool more = startService();
    ///        more;
    ///        {lock} more = endservice(){unlock})
    ///    {
    ///       for buffer returned by serviceNext()
    ///         process buffer
    ///    }
    ///
    /// Synchronization is by an external mutex, to allow other data items to be synchronized by the
    /// same mutex.  Placeholder lock arguments help enforce the synchronization rules.  These
    /// locks are not actually used here.
    ///
    /// Note that the queue can be serviced without synchronization since only one thread will be
    /// doing the work.
    ///
    /// This object does not manage buffer lifetimes.  It assumes
    /// that buffers outlive the collection.
    class SingleServerBufferQueue
    {
    public:
      /// @brief Construct an empty queue.
      SingleServerBufferQueue()
        : busy_(false)
      {
      }

      /// @brief Push a buffer onto the queue.
      ///
      /// The unused scoped lock parameter indicates this method should be protected.
      /// @param buffer is the buffer to be added to the queue
      /// @returns true if if the queue needs to be serviced
      bool push(LinkedBuffer * buffer, boost::mutex::scoped_lock &)
      {
        //std::ostringstream msg;
        //msg << "Q:{"<< (void *) this <<  "} push @" <<(void *)buffer << std::endl;
        //std::cout << msg.str() << std::flush;

        incoming_.push(buffer);
        condition_.notify_one();
        return !busy_;
      }

      /// @brief Prepare to service this queue
      ///
      /// All buffers collected so far will be set aside to be serviced
      /// by this thread.  Any buffers arriving after this call will be
      /// held for later.
      ///
      /// If this method returns true, the calling thread MUST completely
      /// service the queue.
      ///
      /// The unused scoped lock parameter indicates this method should be protected.
      ///
      /// @returns true if if the queue is now ready to be serviced
      bool startService(boost::mutex::scoped_lock &)
      {
        if(busy_)
        {
          return false;
        }
        outgoing_.push(incoming_);
        busy_ = !outgoing_.isEmpty();
        //if(busy_)
        //{
        //  std::ostringstream msg;
        //  msg << "Q:{"<< (void *) this <<  "} startService " << std::endl;
        //  std::cout << msg.str() << std::flush;
        //}
        //else
        //{
        //  std::ostringstream msg;
        //  msg << "Q:{"<< (void *) this <<  "} idle at startService" << std::endl;
        //  std::cout << msg.str() << std::flush;
        //}
        return busy_;
      }

      /// @brief Service the next entry
      ///
      /// No locking is required because the queue should be serviced
      /// by a single thread (the one that set busy_ to true).
      /// @returns the entry to be processed or zero if this batch of entries is finished.
      LinkedBuffer * serviceNext()
      {
        assert(busy_);
        //std::ostringstream msg;
        //msg << "Q:{"<< (void *) this <<  "} pop @" <<(void *)peekOutgoing() << std::endl;
        //std::cout << msg.str() << std::flush;
        return outgoing_.pop();
      }

      /// @brief Service all pending entries at once
      /// @returns the first entry in a linked list of buffers.
      LinkedBuffer * serviceAll()
      {
        assert(busy_);
        //std::ostringstream msg;
        //msg << "Q:{"<< (void *) this <<  "} pop all @" <<(void *)peekOutgoing() << std::endl;
        //std::cout << msg.str() << std::flush;
        return outgoing_.popList();
      }

      /// @brief Relinquish responsibility for servicing the queue
      ///
      /// The unused scoped lock parameter indicates this method should be protected.
      ///
      /// Unless recheck is false, this call will prepare a new batch of buffers
      /// to be processed assuming any arrived while the previous batch was being
      /// serviced.  When there are more buffers, this thread must continue to
      /// process them.
      ///
      /// @param recheck should normally be true indicating that this thread is
      ///        willing to continue servicing the queue.
      /// @param lock unused parameter to be sure the mutex is locked.
      /// @returns true if there are more entries to be serviced.
      bool endService(bool recheck, boost::mutex::scoped_lock & lock)
      {
        assert(busy_);
        busy_ = false;
        if(recheck)
        {
          return startService(lock);
        }
        return false;
      }

      /// @brief Promote buffers from incoming to outgoing (service thread only)
      ///
      /// This method should be called only by the service thread
      /// Any accumulated buffers in the input queue will be appended to the output queue.
      /// If wait is false, then the return value is false if nothing was changed.
      /// if wait is true, then this call waits until some incoming buffers are available.
      /// The (external) mutex must be locked when this method is called (even if wait is false).
      /// @param lock is used for the wait.  It also confirms that the caller has locked the mutex.
      /// @param wait is true if this call should wait for incoming buffers to be available.
      /// @returns true if the incoming buffer queue has changed from empty to populated
      bool refresh(boost::mutex::scoped_lock & lock, bool wait)
      {
        assert(busy_);
        bool wasEmpty = incoming_.isEmpty();
        while(wait && wasEmpty)
        {
          //std::ostringstream msg;
          //msg << "Q:{"<< (void *) this <<  "} wait" << std::endl;
          //std::cout << msg.str() << std::flush;

          condition_.wait(lock);
          wasEmpty = incoming_.isEmpty();
        }
        if(!wasEmpty)
        {
          outgoing_.push(incoming_);
        }
        return !wasEmpty;
      }

      /// @brief A nondestructive peek at the outgoing queue.
      const LinkedBuffer * peekOutgoing()const
      {
        return outgoing_.begin();
      }

      /// @brief Apply a function to every buffer in the queue
      ///
      /// The unused scoped lock parameter indicates this method should be protected.
      ///
      /// @param f is the function to apply
      void apply(boost::function<void (LinkedBuffer *)> f, boost::mutex::scoped_lock &)
      {
        LinkedBuffer * buffer = outgoing_.begin();
        while(buffer != 0)
        {
          f(buffer);
          buffer = buffer->link();
        }
        buffer = incoming_.begin();
        while(buffer != 0)
        {
          f(buffer);
          buffer = buffer->link();
        }
      }
    private:
      BufferQueue incoming_;
      BufferQueue outgoing_;
      boost::condition_variable condition_;
      bool busy_;
      // todo: statistics would be interesting
    };
  }
}
#endif // LINKEDBUFFER_H
