// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifndef SENDER_H
#define SENDER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "Sender_fwd.h"
#include <Communication/BufferRecycler.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief Sender base class for sending outgoing data
    class Sender
    {
    public:
      /// @brief construct
      /// @param recycler to handle empty buffers after their contents have been sent
      Sender(BufferRecycler & recycler)
        : recycler_(recycler)
      {
      }
      virtual ~Sender()
      {
      }

      /// @brief prepare for sending
      virtual void open() = 0;

      /// @brief send a buffer full of data
      virtual void send(LinkedBuffer * buffer) = 0;

      /// @brief tell the sender to stop after completing any pending writes
      virtual void stop() = 0;

      /// @brief clean up resources used by the sender; this cancels outstanding writes
      virtual void close() = 0;

    protected:
      /// @brief return a used/empty buffer to the caller
      void recycle(LinkedBuffer * buffer)
      {
        recycler_.recycle(buffer);
      }

    private:
      BufferRecycler & recycler_;
    };
  }
}
#endif // SENDER_H
