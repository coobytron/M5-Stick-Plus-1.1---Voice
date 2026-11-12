#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <stdint.h>
#include <stddef.h>

#define FILE_READ "r"
#define FILE_WRITE "w"
class File {
 public:
  File() : storage_(nullptr), cursor_(0), writable_(false) {}
  File(std::vector<uint8_t>* storage, bool writable) : storage_(storage), cursor_(0), writable_(writable) {}
  explicit operator bool() const { return storage_ != nullptr; }
  size_t write(const uint8_t* bytes, size_t length) {
    if(!storage_ || !writable_)return 0;
    storage_->insert(storage_->end(),bytes,bytes+length);return length;
  }
  size_t read(uint8_t* bytes, size_t length) {
    if(!storage_)return 0;
    size_t n=storage_->size()-cursor_;
    if(n>length)n=length;
    if(n)std::memcpy(bytes,storage_->data()+cursor_,n);
    cursor_+=n;return n;
  }
  void flush() {}
  void close() { storage_=nullptr; }
 private:
  std::vector<uint8_t>* storage_;
  size_t cursor_;
  bool writable_;
};
class MockSPIFFS {
 public:
  bool working=true;
  std::map<std::string,std::vector<uint8_t>> files;
  bool begin(bool formatOnFail){(void)formatOnFail;return working;}
  bool exists(const char* name) const { return files.find(name)!=files.end(); }
  bool remove(const char* name) {return files.erase(name)>0;}
  bool rename(const char* from,const char* to) {
    auto it=files.find(from);
    if(it==files.end() || exists(to))return false;
    files[to]=it->second;files.erase(it);return true;
  }
  File open(const char* path,const char* mode) {
    if(!working)return File();
    if(mode[0]=='w'){files[path].clear();return File(&files[path],true);}
    auto it=files.find(path);
    return it==files.end()?File():File(&it->second,false);
  }
};
static MockSPIFFS SPIFFS;
