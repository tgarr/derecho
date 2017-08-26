#ifndef PERSIST_LOG_HPP
#define	PERSIST_LOG_HPP

#if !defined(__GNUG__) && !defined(__clang__)
  #error PersistLog.hpp only works with clang and gnu compilers
#endif

#include <stdio.h>
#include <inttypes.h>
#include <map>
#include <set>
#include <string>
#include "PersistException.hpp"
#include "HLC.hpp"

using namespace std;

namespace ns_persistent {

  // Storage type:
  enum StorageType{
    ST_FILE=0,
    ST_MEM,
    ST_3DXP
  };

  //#define INVALID_VERSION ((__int128)-1L)
  #define INVALID_VERSION ((int64_t)-1L)
  #define INVALID_INDEX INT64_MAX

  // index entry for the hlc index
  struct hlc_index_entry {
    HLC hlc;
    int64_t log_idx;
    //default constructor
    hlc_index_entry():log_idx(-1){
    }
    // constructor with hlc and index.
    hlc_index_entry(const HLC & _hlc, const int64_t &_log_idx):
      hlc(_hlc),log_idx(_log_idx) {
    }
    // constructor with time stamp and index.
    hlc_index_entry(const uint64_t &r, const uint64_t &l, const int64_t &_log_idx):
      hlc(r,l),log_idx(_log_idx) {
    }
    //copy constructor
    hlc_index_entry(const struct hlc_index_entry &_entry):
      hlc(_entry.hlc),log_idx(_entry.log_idx){
    }
  };
  // comparator for the hlc index
  struct hlc_index_entry_comp {
    bool operator () (const struct hlc_index_entry & e1, 
      const struct hlc_index_entry & e2) const {
      return e1.hlc < e2.hlc;
    }
  };

  // Persistent log interfaces
  class PersistLog{
  public:
    // LogName
    const string m_sName;
    // HLCIndex
    std::set<hlc_index_entry,hlc_index_entry_comp> hidx;
#ifdef _DEBUG
    void dump_hidx();
#endif//_DEBUG
    // Constructor:
    // Remark: the constructor will check the persistent storage
    // to make sure if this named log(by "name" in the template 
    // parameters) is already there. If it is, load it from disk.
    // Otherwise, create the log.
    PersistLog(const string &name) noexcept(true);
    virtual ~PersistLog() noexcept(true);
    /** Persistent Append
     * @param pdata - serialized data to be append
     * @param size - length of the data
     * @param ver - version of the data, the implementation is responsible for
     *              making sure it grows monotonically.
     * @param mhlc - the hlc clock of the data, the implementation is 
     *               responsible for making sure it grows monotonically.
     * Note that the entry appended can only become persistent till the persist()
     * is called on that entry.
     */
    virtual void append(const void * pdata, 
      const uint64_t & size, const int64_t & ver, //const __int128 & ver, 
      const HLC & mhlc) noexcept(false)=0;

    /**
     * Advance the version number without appendding a log. This is useful
     * to create gap between versions.
     */
    // virtual void advanceVersion(const __int128 & ver) noexcept(false) = 0;
    virtual void advanceVersion(const int64_t & ver) noexcept(false) = 0;

    // Get the length of the log 
    virtual int64_t getLength() noexcept(false) = 0;

    // Get the Earliest Index
    virtual int64_t getEarliestIndex() noexcept(false) = 0;

    // Get the Latest Index
    virtual int64_t getLatestIndex() noexcept(false) = 0;

    // Get the Earlist version
    virtual int64_t getEarliestVersion() noexcept(false) = 0;

    // Get the Latest version
    virtual int64_t getLatestVersion() noexcept(false) = 0;

    // return the last persisted value
    // virtual const __int128 getLastPersisted() noexcept(false) = 0;
    virtual const int64_t getLastPersisted() noexcept(false) = 0;

    // Get a version by entry number
    virtual const void* getEntryByIndex(const int64_t & eno) noexcept(false) = 0;

    // Get the latest version equal or earlier than ver.
    //virtual const void* getEntry(const __int128 & ver) noexcept(false) = 0;
    virtual const void* getEntry(const int64_t & ver) noexcept(false) = 0;

    // Get the latest version - deprecated.
    // virtual const void* getEntry() noexcept(false) = 0;
    // Get a version specified by hlc
    virtual const void* getEntry(const HLC & hlc) noexcept(false) = 0;

    /**
     * Persist the log till specified version
     * @return - the version till which has been persisted.
     * Note that the return value could be higher than the the version asked
     * is lower than the log that has been actually persisted.
     */
    //virtual const __int128 persist() noexcept(false) = 0;
    virtual const int64_t persist() noexcept(false) = 0;

    /**
     * Trim the log till entry number eno, inclusively.
     * For exmaple, there is a log: [7,8,9,4,5,6]. After trim(3), it becomes [5,6]
     * @param eno -  the log number to be trimmed
     */
    virtual void trimByIndex(const int64_t &idx) noexcept(false) = 0;

    /**
     * Trim the log till version, inclusively.
     * @param ver - all log entry before ver will be trimmed.
     */
    virtual void trim(const int64_t &ver) noexcept(false) = 0;

    /**
     * Trim the log till HLC clock, inclusively.
     * @param hlc - all log entry before hlc will be trimmed.
     */
    virtual void trim(const HLC & hlc) noexcept(false) = 0;
  };
}

#endif//PERSIST_LOG_HPP