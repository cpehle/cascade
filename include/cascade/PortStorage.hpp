/*
Copyright 2010, D. E. Shaw Research.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions, and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of D. E. Shaw Research nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

////////////////////////////////////////////////////////////////////////////////
//
// PortStorage.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 11/12/2010
//
// Manages port memory layout and register copies
//
////////////////////////////////////////////////////////////////////////////////

#ifndef PortStorage_hpp_101112062522
#define PortStorage_hpp_101112062522

BEGIN_NAMESPACE_CASCADE

class PortStorage
{
    friend class ClockDomain;
    struct ValueCopy;
public:
    PortStorage ();
    ~PortStorage ();

    //----------------------------------
    // Initialization
    //----------------------------------

    // Add a port, sorted by fifo/synchronous/terminal
    void addPort (PortWrapper *port);

    // Initialize port storage.  For patched register value copies, the src pointer will
    // point to the source port wrapper rather than the source data because the source
    // data might not have been allocated yet for cross-domain patched registers.
    void initPorts (ClockDomain *domain);
    void initFifos (ClockDomain *domain);

    // Resolve the src and dst pointers of all value copies after ports have been
    // initialized in all clock domains.
    void finalizeCopies ();
    void finalizeCopies (stack<ValueCopy> &copies);

    // Determine if a data pointer belongs to the non-fifo data array
    inline bool isOwner (const byte *data)
    {
        return uint64(data - m_portData) < uint64(m_portBytes);
    }

    //----------------------------------
    // Simulation
    //----------------------------------

    // Propagate reset values 
    void propagateReset ();

    // Copy patched register temporaries
    void preTick ();

    // Copy register values
    void tick ();

    // Update port valid flags and zero the pulse ports
    void postTick ();

    // Archive the ports
    void archive (Archive &ar);
    void archvieFifos (Archive &ar);
    void archiveFifoStack (Archive &ar, stack<GenericFifo *> &v);

    // Check for a non-empty fifo with a deactivated consumer
    void checkDeadlock ();

private:
    typedef std::map<uint32, PortList> PortMap;

    // Helper function for port storage allocation.  Returns the number
    // of bytes used/required.
    int allocateValues (const PortMap &ports, int *depthOffset, int *nsize = NULL, byte *storage = NULL);

private:
    //----------------------------------
    // Initialization
    //----------------------------------
    PortList m_fifoPorts;
    PortList m_synchronousPorts;
    PortList m_terminalPorts;

    //----------------------------------
    // Individual value copies
    //----------------------------------

    // A ValueCopy is initialized with dst/src pointing to the port wrappers which
    // are later resolved to actual value pointers in finalizeCopies().  The one
    // exception is wired registers, for which src is initialized with the actual
    // value pointer and does not need to be resolved afterwards.
    struct ValueCopy
    {
        byte *dst;
        byte *src;
        int   size;
    };
    stack<ValueCopy> m_patchedRegs;
    stack<ValueCopy> m_wiredRegs;
    stack<ValueCopy> m_slowRegs;

    //----------------------------------
    // Fifo storage
    //----------------------------------
    byte *m_fifoData;
    int   m_numFifos;
    int   m_fifoDataSize;

    //----------------------------------
    // Port/register storage
    //----------------------------------

    // Port storage is divided into regions based on delay and the type of
    // the terminal port.  Terminal pulse ports with maximum delay k are placed 
    // in the region Pk; latch ports with maximum delay k are placed in the 
    // region Lk; all other ports (normal, patched, wired registers) with 
    // maximum delay k are placed in the region Nk.  Non-terminal ports are then 
    // placed according to their synchronous delay from the terminal port as 
    // shown below.
    //
    // On a rising clock edge the following occurs in order:
    //
    // === pre-tick ==
    // 1. Individual values are copied for patched register temporaries
    // === tick ===
    // 2. A series of memcpys (deepest first, terminal ports last) copies
    //    synchronous values
    // 3. All terminal N ports are invalidated (debug build only)
    // 4. Individual values are copied for wired terminal ports
    // === post-tick ===
    // 5. All terminal P ports are reset to zero
    //
    //
    //                 +--+
    //              +->|N0|
    //              |  +--+
    //              |  |L0|
    //              |  +--+\            /+-+
    //              +->|N1| \          / | |
    //   invalidate |  +--+  |        |  +-+
    //   on tick    |  |L1|  |        |  | |
    //              |  +--+  |        |  +-+\            /+-+
    //              +->|N2|  |        |  | | \          / | |
    //              |  +--+  |        |  +-+  |        |  +-+
    //              |  |L2|  |        |  | |  |        |  | |
    //              |  +--+  |        |  +-+  |        |  +-+\          /+-+
    //              +->|N3|  |        |  | |  |        |  | | |        | | |
    //                 +--+  |        |  +-+  |        |  +-+ |        | +-+
    //                 |L3|  | memcpy |  | |  | memcpy |  | | | memcpy | | |
    //                -+==+  |------->|  +=+  |------->|  +=+ |------->| +=+
    //               / |P3|  |        |  | |  |        |  | | |        | | |
    //              |  +--+  |        |  +-+  |        |  +-+/          \+-+
    //      reset   |  |P2|  |        |  | | /          \ | |
    //      on tick |  +--+  |        |  +-+/            \+-+
    //              |  |P1| /          \ | |
    //              |  +--+/            \+-+
    //               \ |P0|
    //                -+--+
    //
    byte *m_portData;
    int   m_portBytes;
    byte *m_pulsePorts;     // Pointer into m_portData array
    int   m_pulsePortBytes; // Size in bytes of terminal pulse port storage
    int   m_maxDelay;

    stack<ValueCopy> m_regCopies; // Memcpys, sorted from deepest to shallowest

    // For each delay we store a "delay offset" with the property that if Pk is 
    // connected to P0 with delay k and P0 has offset i0 within the level-0 ports, 
    // then Pk has offset i0 + (delay offset k) within the port data array.  
    int *m_delayOffset;

    // In debug builds we need to keep track of the level-0 N regions so that
    // we can invalidate those ports on a rising clock edge.
#ifdef _DEBUG
    struct Region
    {
        byte *data;
        int  size;
    };
    stack<Region> m_nports;
#endif

    //----------------------------------
    // Value iterator
    //----------------------------------

    // Values are stored in blocks with the following format:
    //
    //    +----------+
    //    | size:16  |
    //    +----------+
    //    | count:16 |
    //    +----------+
    //    |  value1  |
    //    +----------+
    //    |  value2  |
    //    +----------+
    //        ...
    // Values are aligned to two bytes for size = 2 and to four bytes for size >= 4.
    // In release builds the values are consecutive; in debug builds each value
    // is immediately preceded by a "flags" byte.  ValueIterator is a helper class to 
    // iterate over values across multiple blocks.
    class ValueIterator
    {
        DECLARE_NOCOPY(ValueIterator);
    public:
        ValueIterator (byte *start, int len) : 
          m_curr(start), 
              m_end(start + len),
              m_count(0), //initialized in StartBlock
              m_size(0),  //initialized in StartBlock
              m_stride(0) //initialized in StartBlock
          {
              if (len)
                  startBlock();
          }

          inline operator bool () 
          { 
              return m_curr < m_end; 
          }
          inline uint16 size() const 
          { 
              return m_size; 
          }
          inline byte *value() 
          { 
              return m_curr; 
          }
#ifdef _DEBUG
          inline byte &flags() 
          { 
              return m_curr[-1]; 
          }
#endif
          inline void operator++ (int) 
          {
              if (!--m_count)
              {
                  m_curr = (byte *) ((intptr_t(m_curr) + m_size + 3) & ~intptr_t(3));
                  if (m_curr < m_end)
                      startBlock();
              }
              else
                  m_curr += m_stride;
          }

    private:
        inline void startBlock ()
        {
            m_size = *(uint16*)m_curr;
            m_count = *(uint16*)(m_curr+2);
            assert(m_size);
#ifdef _DEBUG
            m_stride = (m_size < 4) ? (2 * m_size) : (m_size + 4);
            m_curr += (4 + m_stride - m_size);
#else
            m_stride = m_size;
            m_curr += 4;
#endif
        }

    private:
        byte *m_curr;    // Current location
        byte *m_end;     // End of values
        uint16 m_count;  // # Remaining values in current block (0 = 0x10000)
        uint16 m_size;   // Size of values in current block
        int m_stride;    // Total size of value + flags
    };
};

END_NAMESPACE_CASCADE

#endif // #ifndef PortStorage_hpp_101112062522

