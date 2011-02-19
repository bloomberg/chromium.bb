/*****************************************************************************
Copyright (c) 2001 - 2009, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 05/05/2009
*****************************************************************************/

#ifdef WIN32
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #ifdef LEGACY_WIN32
      #include <wspiapi.h>
   #endif
#endif

#include <cstring>
#include "cache.h"
#include "core.h"

using namespace std;

CCache::CCache(const int& size):
m_iMaxSize(size),
m_iHashSize(size * 3),
m_iCurrSize(0)
{
   m_vHashPtr.resize(m_iHashSize);

   #ifndef WIN32
      pthread_mutex_init(&m_Lock, NULL);
   #else
      m_Lock = CreateMutex(NULL, false, NULL);
   #endif
}

CCache::~CCache()
{
   for (list<CCacheItem*>::iterator i = m_StorageList.begin(); i != m_StorageList.end(); ++ i)
      delete *i;
   m_StorageList.clear();

   #ifndef WIN32
      pthread_mutex_destroy(&m_Lock);
   #else
      CloseHandle(m_Lock);
   #endif
}

int CCache::lookup(CCacheItem* data)
{
   CGuard cacheguard(m_Lock);

   int key = data->getKey();

   if (key < 0)
      return -1;

   if (key >= m_iMaxSize)
      key %= m_iHashSize;

   for (list<list<CCacheItem*>::iterator>::iterator i = m_vHashPtr[key].begin(); i != m_vHashPtr[key].end(); ++ i)
   {
      if (*data == ***i)
      {
         // copy the cached info
         *data = ***i;
         return 0;
      }
   }

   return -1;
}

int CCache::update(CCacheItem* data)
{
   CGuard cacheguard(m_Lock);

   int key = data->getKey();

   if (key < 0)
      return -1;

   if (key >= m_iMaxSize)
      key %= m_iHashSize;

   CCacheItem* curr = NULL;

   for (list<list<CCacheItem*>::iterator>::iterator i = m_vHashPtr[key].begin(); i != m_vHashPtr[key].end(); ++ i)
   {
      if (*data == ***i)
      {
         // update the existing entry with the new value
         ***i = *data;
         curr = **i;

         // remove the current entry
         m_StorageList.erase(*i);
         m_vHashPtr[key].erase(i);

         // re-insert to the front
         m_StorageList.push_front(curr);
         m_vHashPtr[key].push_front(m_StorageList.begin());

         return 0;
      }
   }

   // create new entry and insert to front
   curr = data->clone();
   m_StorageList.push_front(curr);
   m_vHashPtr[key].push_front(m_StorageList.begin());

   ++ m_iCurrSize;
   if (m_iCurrSize >= m_iMaxSize)
   {
      CCacheItem* last_data = m_StorageList.back();
      int last_key = last_data->getKey() % m_iHashSize;

      for (list<list<CCacheItem*>::iterator>::iterator i = m_vHashPtr[last_key].begin(); i != m_vHashPtr[last_key].end(); ++ i)
      {
         if (*last_data == ***i)
         {
            m_vHashPtr[last_key].erase(i);
            break;
         }
      }

      delete last_data;
      m_StorageList.pop_back();
      -- m_iCurrSize;
   }

   return 0;
}


CInfoBlock& CInfoBlock::operator=(CCacheItem& obj)
{
   try
   {
      const CInfoBlock& real_obj = dynamic_cast<CInfoBlock&>(obj);

      std::copy(real_obj.m_piIP, real_obj.m_piIP + 3, m_piIP);
      m_iIPversion = real_obj.m_iIPversion;
      m_ullTimeStamp = real_obj.m_ullTimeStamp;
      m_iRTT = real_obj.m_iRTT;
      m_iBandwidth = real_obj.m_iBandwidth;
      m_iLossRate = real_obj.m_iLossRate;
      m_iReorderDistance = real_obj.m_iReorderDistance;
      m_dInterval = real_obj.m_dInterval;
      m_dCWnd = real_obj.m_dCWnd;
   }
   catch (...)
   {
   }

   return *this;
}

bool CInfoBlock::operator==(CCacheItem& obj)
{
   try
   {
      const CInfoBlock& real_obj = dynamic_cast<CInfoBlock&>(obj);

      if (m_iIPversion != real_obj.m_iIPversion)
         return false;

      else if (m_iIPversion == AF_INET)
         return (m_piIP[0] == real_obj.m_piIP[0]);

      for (int i = 0; i < 4; ++ i)
      {
         if (m_piIP[i] != real_obj.m_piIP[i])
            return false;
      }
   }
   catch (...)
   {
      return false;
   }

   return true;}

CInfoBlock* CInfoBlock::clone()
{
   CInfoBlock* obj = new CInfoBlock;

   std::copy(m_piIP, m_piIP + 3, obj->m_piIP);
   obj->m_iIPversion = m_iIPversion;
   obj->m_ullTimeStamp = m_ullTimeStamp;
   obj->m_iRTT = m_iRTT;
   obj->m_iBandwidth = m_iBandwidth;
   obj->m_iLossRate = m_iLossRate;
   obj->m_iReorderDistance = m_iReorderDistance;
   obj->m_dInterval = m_dInterval;
   obj->m_dCWnd = m_dCWnd;

   return obj;
}

int CInfoBlock::getKey()
{
   if (m_iIPversion == AF_INET)
      return m_piIP[0];

   return m_piIP[0] + m_piIP[1] + m_piIP[2] + m_piIP[3];
}

void CInfoBlock::convert(const sockaddr* addr, const int& ver, uint32_t ip[])
{
   if (ver == AF_INET)
   {
      ip[0] = ((sockaddr_in*)addr)->sin_addr.s_addr;
      ip[1] = ip[2] = ip[3] = 0;
   }
   else
   {
      memcpy((char*)ip, (char*)((sockaddr_in6*)addr)->sin6_addr.s6_addr, 16);
   }
}
