#ifndef __TRACYPROFILER_HPP__
#define __TRACYPROFILER_HPP__

#include <assert.h>
#include <atomic>
#include <chrono>
#include <stdint.h>
#include <string.h>

#include "concurrentqueue.h"
#include "TracyCallstack.hpp"
#include "TracySysTime.hpp"
#include "TracyFastVector.hpp"
#include "../common/TracyQueue.hpp"
#include "../common/TracyAlign.hpp"
#include "../common/TracyAlloc.hpp"
#include "../common/TracyMutex.hpp"
#include "../common/TracySystem.hpp"

#if defined _WIN32 || defined __CYGWIN__
#  include <intrin.h>
#endif
#ifdef __APPLE__
#  include <TargetConditionals.h>
#  include <mach/mach_time.h>
#endif

#if defined _WIN32 || defined __CYGWIN__ || ( ( defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64 ) && !defined __ANDROID__ ) || __ARM_ARCH >= 6
#  define TRACY_HW_TIMER
#  if defined _WIN32 || defined __CYGWIN__
     // Enable optimization for MSVC __rdtscp() intrin, saving one LHS of a cpu value on the stack.
     // This comes at the cost of an unaligned memory write.
#    define TRACY_RDTSCP_OPT
#  endif
#endif

#ifndef TracyConcat
#  define TracyConcat(x,y) TracyConcatIndirect(x,y)
#endif
#ifndef TracyConcatIndirect
#  define TracyConcatIndirect(x,y) x##y
#endif

namespace tracy
{

class GpuCtx;
class Profiler;
class Socket;

struct GpuCtxWrapper
{
    GpuCtx* ptr;
};

moodycamel::ConcurrentQueue<QueueItem>::ExplicitProducer* GetToken();
Profiler& GetProfiler();
std::atomic<uint32_t>& GetLockCounter();
std::atomic<uint8_t>& GetGpuCtxCounter();
GpuCtxWrapper& GetGpuCtx();

void InitRPMallocThread();

struct SourceLocationData
{
    const char* name;
    const char* function;
    const char* file;
    uint32_t line;
    uint32_t color;
};

#ifdef TRACY_ON_DEMAND
struct LuaZoneState
{
    uint32_t counter;
    bool active;
};
#endif

using Magic = moodycamel::ConcurrentQueueDefaultTraits::index_t;

#if __ARM_ARCH >= 6 && !defined TARGET_OS_IOS
extern int64_t (*GetTimeImpl)();
#endif


class Profiler
{
public:
    Profiler();
    ~Profiler();

    static tracy_force_inline int64_t GetTime( uint32_t& cpu )
    {
#ifdef TRACY_HW_TIMER
#  if TARGET_OS_IOS == 1
        cpu = 0xFFFFFFFF;
        return mach_absolute_time();
#  elif __ARM_ARCH >= 6
        cpu = 0xFFFFFFFF;
        return GetTimeImpl();
#  elif defined _WIN32 || defined __CYGWIN__
        const auto t = int64_t( __rdtscp( &cpu ) );
        return t;
#  elif defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64
        uint32_t eax, edx;
        asm volatile ( "rdtscp" : "=a" (eax), "=d" (edx), "=c" (cpu) :: );
        return ( uint64_t( edx ) << 32 ) + uint64_t( eax );
#  endif
#else
        cpu = 0xFFFFFFFF;
        return std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::high_resolution_clock::now().time_since_epoch() ).count();
#endif
    }

    static tracy_force_inline int64_t GetTime()
    {
#ifdef TRACY_HW_TIMER
#  if TARGET_OS_IOS == 1
        return mach_absolute_time();
#  elif __ARM_ARCH >= 6
        return GetTimeImpl();
#  elif defined _WIN32 || defined __CYGWIN__
        unsigned int dontcare;
        const auto t = int64_t( __rdtscp( &dontcare ) );
        return t;
#  elif defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64
        uint32_t eax, edx;
        asm volatile ( "rdtscp" : "=a" (eax), "=d" (edx) :: "%ecx" );
        return ( uint64_t( edx ) << 32 ) + uint64_t( eax );
#  endif
#else
        return std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::high_resolution_clock::now().time_since_epoch() ).count();
#endif
    }

    tracy_force_inline uint32_t GetNextZoneId()
    {
        return m_zoneId.fetch_add( 1, std::memory_order_relaxed );
    }

    static tracy_force_inline void SendFrameMark( const char* name )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::FrameMarkMsg );
        MemWrite( &item->frameMark.time, GetTime() );
        MemWrite( &item->frameMark.name, uint64_t( name ) );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void SendFrameMark( const char* name, QueueType type )
    {
        assert( type == QueueType::FrameMarkMsgStart || type == QueueType::FrameMarkMsgEnd );
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        GetProfiler().m_serialLock.lock();
        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, type );
        MemWrite( &item->frameMark.time, GetTime() );
        MemWrite( &item->frameMark.name, uint64_t( name ) );
        GetProfiler().m_serialQueue.commit_next();
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void PlotData( const char* name, int64_t val )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::PlotData );
        MemWrite( &item->plotData.name, (uint64_t)name );
        MemWrite( &item->plotData.time, GetTime() );
        MemWrite( &item->plotData.type, PlotDataType::Int );
        MemWrite( &item->plotData.data.i, val );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void PlotData( const char* name, float val )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::PlotData );
        MemWrite( &item->plotData.name, (uint64_t)name );
        MemWrite( &item->plotData.time, GetTime() );
        MemWrite( &item->plotData.type, PlotDataType::Float );
        MemWrite( &item->plotData.data.f, val );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void PlotData( const char* name, double val )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::PlotData );
        MemWrite( &item->plotData.name, (uint64_t)name );
        MemWrite( &item->plotData.time, GetTime() );
        MemWrite( &item->plotData.type, PlotDataType::Double );
        MemWrite( &item->plotData.data.d, val );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void Message( const char* txt, size_t size )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto ptr = (char*)tracy_malloc( size+1 );
        memcpy( ptr, txt, size );
        ptr[size] = '\0';
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::Message );
        MemWrite( &item->message.time, GetTime() );
        MemWrite( &item->message.thread, GetThreadHandle() );
        MemWrite( &item->message.text, (uint64_t)ptr );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void Message( const char* txt )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::MessageLiteral );
        MemWrite( &item->message.time, GetTime() );
        MemWrite( &item->message.thread, GetThreadHandle() );
        MemWrite( &item->message.text, (uint64_t)txt );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void MessageColor( const char* txt, size_t size, uint32_t color )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto ptr = (char*)tracy_malloc( size+1 );
        memcpy( ptr, txt, size );
        ptr[size] = '\0';
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::MessageColor );
        MemWrite( &item->messageColor.time, GetTime() );
        MemWrite( &item->messageColor.thread, GetThreadHandle() );
        MemWrite( &item->messageColor.text, (uint64_t)ptr );
        MemWrite( &item->messageColor.r, uint8_t( ( color       ) & 0xFF ) );
        MemWrite( &item->messageColor.g, uint8_t( ( color >> 8  ) & 0xFF ) );
        MemWrite( &item->messageColor.b, uint8_t( ( color >> 16 ) & 0xFF ) );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void MessageColor( const char* txt, uint32_t color )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::MessageLiteralColor );
        MemWrite( &item->messageColor.time, GetTime() );
        MemWrite( &item->messageColor.thread, GetThreadHandle() );
        MemWrite( &item->messageColor.text, (uint64_t)txt );
        MemWrite( &item->messageColor.r, uint8_t( ( color       ) & 0xFF ) );
        MemWrite( &item->messageColor.g, uint8_t( ( color >> 8  ) & 0xFF ) );
        MemWrite( &item->messageColor.b, uint8_t( ( color >> 16 ) & 0xFF ) );
        tail.store( magic + 1, std::memory_order_release );
    }

    static tracy_force_inline void MemAlloc( const void* ptr, size_t size )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        const auto thread = GetThreadHandle();

        GetProfiler().m_serialLock.lock();
        SendMemAlloc( QueueType::MemAlloc, thread, ptr, size );
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void MemFree( const void* ptr )
    {
#ifdef TRACY_ON_DEMAND
        if( !GetProfiler().IsConnected() ) return;
#endif
        const auto thread = GetThreadHandle();

        GetProfiler().m_serialLock.lock();
        SendMemFree( QueueType::MemFree, thread, ptr );
        GetProfiler().m_serialLock.unlock();
    }

    static tracy_force_inline void MemAllocCallstack( const void* ptr, size_t size, int depth )
    {
        auto& profiler = GetProfiler();
#ifdef TRACY_HAS_CALLSTACK
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto thread = GetThreadHandle();

        rpmalloc_thread_initialize();
        auto callstack = Callstack( depth );

        profiler.m_serialLock.lock();
        SendMemAlloc( QueueType::MemAllocCallstack, thread, ptr, size );
        SendCallstackMemory( callstack );
        profiler.m_serialLock.unlock();
#else
        MemAlloc( ptr, size );
#endif
    }

    static tracy_force_inline void MemFreeCallstack( const void* ptr, int depth )
    {
        auto& profiler = GetProfiler();
#ifdef TRACY_HAS_CALLSTACK
#  ifdef TRACY_ON_DEMAND
        if( !profiler.IsConnected() ) return;
#  endif
        const auto thread = GetThreadHandle();

        rpmalloc_thread_initialize();
        auto callstack = Callstack( depth );

        profiler.m_serialLock.lock();
        SendMemFree( QueueType::MemFreeCallstack, thread, ptr );
        SendCallstackMemory( callstack );
        profiler.m_serialLock.unlock();
#else
        MemFree( ptr );
#endif
    }

    static tracy_force_inline void SendCallstack( int depth, uint64_t thread )
    {
#ifdef TRACY_HAS_CALLSTACK
        auto ptr = Callstack( depth );
        Magic magic;
        auto token = GetToken();
        auto& tail = token->get_tail_index();
        auto item = token->enqueue_begin<tracy::moodycamel::CanAlloc>( magic );
        MemWrite( &item->hdr.type, QueueType::Callstack );
        MemWrite( &item->callstack.ptr, ptr );
        MemWrite( &item->callstack.thread, thread );
        tail.store( magic + 1, std::memory_order_release );
#endif
    }

    void SendCallstack( int depth, uint64_t thread, const char* skipBefore );
    static void CutCallstack( void* callstack, const char* skipBefore );

    static bool ShouldExit();

#ifdef TRACY_ON_DEMAND
    tracy_force_inline bool IsConnected()
    {
        return m_isConnected.load( std::memory_order_relaxed );
    }

    tracy_force_inline void DeferItem( const QueueItem& item )
    {
        m_deferredLock.lock();
        auto dst = m_deferredQueue.push_next();
        memcpy( dst, &item, sizeof( item ) );
        m_deferredLock.unlock();
    }
#endif

    void RequestShutdown() { m_shutdown.store( true, std::memory_order_relaxed ); m_shutdownManual.store( true, std::memory_order_relaxed ); }
    bool HasShutdownFinished() const { return m_shutdownFinished.load( std::memory_order_relaxed ); }

private:
    enum DequeueStatus { Success, ConnectionLost, QueueEmpty };

    static void LaunchWorker( void* ptr ) { ((Profiler*)ptr)->Worker(); }
    void Worker();

    void ClearQueues( tracy::moodycamel::ConsumerToken& token );
    DequeueStatus Dequeue( tracy::moodycamel::ConsumerToken& token );
    DequeueStatus DequeueSerial();
    bool AppendData( const void* data, size_t len );
    bool CommitData();
    bool NeedDataSize( size_t len );

    tracy_force_inline void AppendDataUnsafe( const void* data, size_t len )
    {
        memcpy( m_buffer + m_bufferOffset, data, len );
        m_bufferOffset += int( len );
    }

    bool SendData( const char* data, size_t len );
    void SendString( uint64_t ptr, const char* str, QueueType type );
    void SendSourceLocation( uint64_t ptr );
    void SendSourceLocationPayload( uint64_t ptr );
    void SendCallstackPayload( uint64_t ptr );
    void SendCallstackAlloc( uint64_t ptr );
    void SendCallstackFrame( uint64_t ptr );

    bool HandleServerQuery();

    void CalibrateTimer();
    void CalibrateDelay();

    static tracy_force_inline void SendCallstackMemory( void* ptr )
    {
#ifdef TRACY_HAS_CALLSTACK
        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, QueueType::CallstackMemory );
        MemWrite( &item->callstackMemory.ptr, (uint64_t)ptr );
        GetProfiler().m_serialQueue.commit_next();
#endif
    }

    static tracy_force_inline void SendMemAlloc( QueueType type, const uint64_t thread, const void* ptr, size_t size )
    {
        assert( type == QueueType::MemAlloc || type == QueueType::MemAllocCallstack );

        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, type );
        MemWrite( &item->memAlloc.time, GetTime() );
        MemWrite( &item->memAlloc.thread, thread );
        MemWrite( &item->memAlloc.ptr, (uint64_t)ptr );
        if( compile_time_condition<sizeof( size ) == 4>::value )
        {
            memcpy( &item->memAlloc.size, &size, 4 );
            memset( &item->memAlloc.size + 4, 0, 2 );
        }
        else
        {
            assert( sizeof( size ) == 8 );
            memcpy( &item->memAlloc.size, &size, 6 );
        }
        GetProfiler().m_serialQueue.commit_next();
    }

    static tracy_force_inline void SendMemFree( QueueType type, const uint64_t thread, const void* ptr )
    {
        assert( type == QueueType::MemFree || type == QueueType::MemFreeCallstack );

        auto item = GetProfiler().m_serialQueue.prepare_next();
        MemWrite( &item->hdr.type, type );
        MemWrite( &item->memFree.time, GetTime() );
        MemWrite( &item->memFree.thread, thread );
        MemWrite( &item->memFree.ptr, (uint64_t)ptr );
        GetProfiler().m_serialQueue.commit_next();
    }

    double m_timerMul;
    uint64_t m_resolution;
    uint64_t m_delay;
    std::atomic<int64_t> m_timeBegin;
    uint64_t m_mainThread;
    uint64_t m_epoch;
    std::atomic<bool> m_shutdown;
    std::atomic<bool> m_shutdownManual;
    std::atomic<bool> m_shutdownFinished;
    Socket* m_sock;
    bool m_noExit;
    std::atomic<uint32_t> m_zoneId;

    void* m_stream;     // LZ4_stream_t*
    char* m_buffer;
    int m_bufferOffset;
    int m_bufferStart;

    QueueItem* m_itemBuf;
    char* m_lz4Buf;

    FastVector<QueueItem> m_serialQueue, m_serialDequeue;
    TracyMutex m_serialLock;

#ifdef TRACY_ON_DEMAND
    std::atomic<bool> m_isConnected;
    std::atomic<uint64_t> m_frameCount;

    TracyMutex m_deferredLock;
    FastVector<QueueItem> m_deferredQueue;
#endif

#ifdef TRACY_HAS_SYSTIME
    void ProcessSysTime();

    SysTime m_sysTime;
    uint64_t m_sysTimeLast = 0;
#else
    void ProcessSysTime() {}
#endif
};

};

#endif
