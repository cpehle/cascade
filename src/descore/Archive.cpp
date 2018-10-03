/*
Copyright 2007, D. E. Shaw Research.
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

//////////////////////////////////////////////////////////////////////
//
// Archive.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Archive.hpp"
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <zlib/zlib.h>
#else
#include <zlib.h>
#endif

#define COMPRESSION_BLOCK_SIZE 0x100000
#define ARCHIVE_VERSION        1.0

////////////////////////////////////////////////////////////////////////
//
// Archive()
//
////////////////////////////////////////////////////////////////////////
Archive::Archive (const string &filename, int mode) 
: m_f(NULL),
m_allowReadPastEOF(false),
m_data(new byte[COMPRESSION_BLOCK_SIZE]),
m_compressed(new byte[compressBound(COMPRESSION_BLOCK_SIZE)])
{
    memset(m_data, 0, COMPRESSION_BLOCK_SIZE);
    memset(m_compressed, 0, compressBound(COMPRESSION_BLOCK_SIZE));
    open(filename, mode);
}

/////////////////////////////////////////////////////////////////
//
// Archive()
//
/////////////////////////////////////////////////////////////////
Archive::Archive ()
: m_f(NULL),
m_allowReadPastEOF(false),
m_data(new byte[COMPRESSION_BLOCK_SIZE]),
m_compressed(new byte[compressBound(COMPRESSION_BLOCK_SIZE)])
{
    memset(m_data, 0, COMPRESSION_BLOCK_SIZE);
    memset(m_compressed, 0, compressBound(COMPRESSION_BLOCK_SIZE));
}

/////////////////////////////////////////////////////////////////
//
// open()
//
/////////////////////////////////////////////////////////////////
void Archive::open (const string &filename, int mode)
{
    assert_always(!m_f, "Archive is already open");

    m_validationError = false;
    m_safeMode = false;
    m_isLoading = ((mode & 3) == Load);
    bool validate = ((mode & 3) == Validate);
    m_isReading = m_isLoading || validate;
    m_allowReadPastEOF = ((mode & AllowReadPastEOF) != 0);
    assert_always(m_isLoading || !m_allowReadPastEOF, "AllowPastReadEOF can only be specified with Archive::Load");
    m_checkval = (byte) 0xec;
    m_dataIndex = m_isReading ? COMPRESSION_BLOCK_SIZE : 0;
    m_numBytes = 0;

    m_f = ::fopen(*filename, m_isReading ? "rb" : "wb");
    assert_always(m_f, "Could not open %s", *filename);
    m_fnArchivePrimitive = validate ? &Archive::validatePrimitive :
        m_isLoading ? &Archive::loadPrimitive : &Archive::savePrimitive;

    // Archive the version
    float version = ARCHIVE_VERSION;
    *this | version;
    assert_always(version == ARCHIVE_VERSION, 
        "Archive version mismatch: current version is %f but file %s has version %f",
        ARCHIVE_VERSION, *filename, version);

    // Archive the safe mode flag.  Do this while m_safeMode is still
    // false so that the flag itself never has a checkval.
    if (m_isReading)
    {
        loadPrimitive(&m_safeMode, 1);
    }
    else
    {
        bool safeMode = (mode == SafeStore);
        savePrimitive(&safeMode, 1);
        m_safeMode = safeMode;
    }
}

////////////////////////////////////////////////////////////////////////
//
// ~Archive()
//
////////////////////////////////////////////////////////////////////////
Archive::~Archive ()
{
    close();
    delete[] m_data;
    delete[] m_compressed;
}

/////////////////////////////////////////////////////////////////
//
// close()
//
/////////////////////////////////////////////////////////////////
void Archive::close ()
{
    if (m_f)
    {
        if (!m_isReading)
            writeBlock();
        ::fflush(m_f);
        ::fclose(m_f);
    }
    m_f = NULL;
}

/////////////////////////////////////////////////////////////////
//
// operator bool ()
//
/////////////////////////////////////////////////////////////////
Archive::operator bool ()
{
    if (!m_f)
        return false;
    if (!m_isLoading)
        return true;
    if (m_dataIndex == COMPRESSION_BLOCK_SIZE)
        readBlock();
    return (m_dataIndex < m_numBytes);
}

/////////////////////////////////////////////////////////////////
//
// archiveData()
//
/////////////////////////////////////////////////////////////////
void Archive::archiveData (void *buff, int size)
{
    if (size) 
    {
        assert_always(buff);
        (this->*m_fnArchivePrimitive)(buff, size); 
    }
}

////////////////////////////////////////////////////////////////////////
//
// archiveCheckval()
//
////////////////////////////////////////////////////////////////////////
void Archive::archiveCheckval (char inc)
{
    if (m_safeMode)
    {
        if (m_isReading)
        {
            char c;
            read(&c, 1);
            if (c != m_checkval)
                die("Data is not being loaded with the same order/size that it was stored");
        }
        else
            write(&m_checkval, 1);
        m_checkval += inc;
    }
}

////////////////////////////////////////////////////////////////////////
//
// savePrimitive()
//
////////////////////////////////////////////////////////////////////////
void Archive::savePrimitive (void *buff, int size) 
{
    assert(m_f);
    archiveCheckval(size);
    write(buff, size);
}

////////////////////////////////////////////////////////////////////////
//
// loadPrimitive()
//
////////////////////////////////////////////////////////////////////////
void Archive::loadPrimitive (void *buff, int size) 
{
    assert(m_f);
    archiveCheckval(size);
    read(buff, size);
}

////////////////////////////////////////////////////////////////////////
//
// validatePrimitive()
//
////////////////////////////////////////////////////////////////////////
void Archive::validatePrimitive (void *buff, int size) 
{
    byte stack_buff[256];
    byte *pbuff = (size <= (int) sizeof(stack_buff)) ? stack_buff : new byte[size];

    assert(m_f);
    archiveCheckval(size);
    read(pbuff, size);
    if (memcmp(buff, pbuff, size))
        m_validationError = true;

    if (size > (int) sizeof(stack_buff))
        delete pbuff;
}

/////////////////////////////////////////////////////////////////
//
// write()
//
/////////////////////////////////////////////////////////////////
void Archive::write (const void *buff, int size)
{
    const byte *data = (const byte *) buff;

    while (size)
    {
        // Flush the buffer if we need to
        if (m_dataIndex == COMPRESSION_BLOCK_SIZE)
            writeBlock();

        // Write what we can to the uncompressed buffer
        int len = COMPRESSION_BLOCK_SIZE - m_dataIndex;
        if (len > size)
            len = size;
        memcpy(m_data + m_dataIndex, data, len);
        data += len;
        size -= len;
        m_dataIndex += len;
    }
}

/////////////////////////////////////////////////////////////////
//
// read()
//
/////////////////////////////////////////////////////////////////
void Archive::read (void *buff, int size)
{
    byte *data = (byte *) buff;

    while (size)
    {
        // Fill the buffer if we need to
        if (m_dataIndex == COMPRESSION_BLOCK_SIZE)
            readBlock();

        // Read what we can from the uncompressed buffer
        int len = m_numBytes - m_dataIndex;
        assert_always(len || m_allowReadPastEOF, "Attempted to read past end of archive");
        if (!len)
        {
            memset(data, 0, size);
            return;
        }
        if (len > size)
            len = size;
        memcpy(data, m_data + m_dataIndex, len);
        data += len;
        size -= len;
        m_dataIndex += len;
    }
}

/////////////////////////////////////////////////////////////////
//
// writeBlock()
//
/////////////////////////////////////////////////////////////////
void Archive::writeBlock ()
{
    uLong destLen = compressBound(COMPRESSION_BLOCK_SIZE);
    compress(m_compressed, &destLen, m_data, m_dataIndex);
    ::fwrite(&destLen, 4, 1, m_f);
    ::fwrite(m_compressed, 1, destLen, m_f);
    m_dataIndex = 0;
}

/////////////////////////////////////////////////////////////////
//
// readBlock()
//
/////////////////////////////////////////////////////////////////
void Archive::readBlock ()
{
    m_dataIndex = 0;
    m_numBytes = 0;

    uLong srcLen = 0;
    if (!::fread(&srcLen, 4, 1, m_f))
        return;
    if (!::fread(m_compressed, 1, srcLen, m_f))
        return;
    uLong destLen = COMPRESSION_BLOCK_SIZE;
    if (uncompress(m_data, &destLen, m_compressed, srcLen) != Z_OK)
        return;
    m_numBytes = (int) destLen;
}

/////////////////////////////////////////////////////////////////
//
// string
//
/////////////////////////////////////////////////////////////////
Archive &operator| (Archive &ar, string &str)
{
    int len = (int) str.length();
    ar | len;
    if (ar.isLoading())
    {
        char *buff = new char[len + 1];
        ar.archiveData(buff, len + 1);
        str = buff;
    }
    else
    {
        ar.archiveData((void *) *str, len + 1);
    }
    return ar;
}

Archive &operator| (Archive &ar, const string &str)
{
    return ar | (string &) str;
}

Archive &operator| (Archive &ar, const char *sz)
{
    if (ar.isLoading())
    {
        string s;
        ar | s;
        assert_always(s == sz, "Failure reading archive:\n    expected \"%s\" but read \"%s\"", sz, *s);
    }
    else
    {
        string s(sz);
        ar | s;
    }
    return ar;
}

Archive &operator<< (Archive &ar, const char *sz)
{
    assert_always(!ar.isLoading());
    string s(sz);
    return ar | s;
}

