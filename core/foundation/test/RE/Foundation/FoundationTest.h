/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationTest.h

    The whole test harness. Deliberately about a hundred lines and dependency-free - a test
    framework is exactly the kind of thing that is easier to own than to depend on, and every
    feature a real framework has beyond this is one we do not currently need.

    Tests are plain functions registered by hand in FoundationTestMain.c. No auto-registration:
    doing it portably in C means either constructor attributes or linker-section tricks, and an
    explicit list that someone has to edit is a fair price for a build that behaves the same
    everywhere.

    A failed check reports and keeps going within its test, so one run surfaces every failure
    rather than only the first.
*/

typedef void ( *ReTestFn )( void );

/* Called by the checks below; not usually called directly. */
void RE_Test_ReportFailure( const char *file, ReUint32 line, const char *format, ... );

/* Runs one test, printing its name and result. Accumulates into the global tallies. */
void RE_Test_Run( const char *name, ReTestFn fn );

/* Prints the summary. Returns 0 if everything passed, 1 otherwise - i.e. a process exit code. */
ReSint32 RE_Test_Summary( void );

#define RE_TEST_CHECK( condition )                                                      \
    do                                                                                  \
    {                                                                                   \
        if ( !( condition ) )                                                           \
        {                                                                               \
            RE_Test_ReportFailure( __FILE__, __LINE__, "expected: %s", #condition );    \
        }                                                                               \
    }                                                                                   \
    while ( 0 )

#define RE_TEST_CHECK_EQ_UINT( actual, expected )                                       \
    do                                                                                  \
    {                                                                                   \
        ReUint64 RE_TEST_CHECK_EQ_UINT_a = (ReUint64) ( actual );                       \
        ReUint64 RE_TEST_CHECK_EQ_UINT_e = (ReUint64) ( expected );                     \
        if ( RE_TEST_CHECK_EQ_UINT_a != RE_TEST_CHECK_EQ_UINT_e )                       \
        {                                                                               \
            RE_Test_ReportFailure( __FILE__, __LINE__, "%s: got %llu, expected %llu",   \
                #actual, RE_TEST_CHECK_EQ_UINT_a, RE_TEST_CHECK_EQ_UINT_e );            \
        }                                                                               \
    }                                                                                   \
    while ( 0 )

#define RE_TEST_CHECK_NOT_NULL( pointer )                                               \
    do                                                                                  \
    {                                                                                   \
        if ( ( pointer ) == 0 )                                                         \
        {                                                                               \
            RE_Test_ReportFailure( __FILE__, __LINE__, "%s was null", #pointer );       \
        }                                                                               \
    }                                                                                   \
    while ( 0 )

#define RE_TEST_CHECK_NULL( pointer )                                                   \
    do                                                                                  \
    {                                                                                   \
        if ( ( pointer ) != 0 )                                                         \
        {                                                                               \
            RE_Test_ReportFailure( __FILE__, __LINE__, "%s was not null", #pointer );   \
        }                                                                               \
    }                                                                                   \
    while ( 0 )

/* Test entry points, one per source file. */
void RE_Test_VirtualMemory( void );
void RE_Test_MemoryMetadata( void );
void RE_Test_SpinLock( void );
void RE_Test_MemoryArena( void );
void RE_Test_MemoryScratch( void );
