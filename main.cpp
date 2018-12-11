//
//  main.cpp
//  HAR
//
//  Created by Hossein Yousefi on 11/12/2018.
//  Copyright © 2018 Harbour Space. All rights reserved.
//

#include <cstdio>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

constexpr size_t kMaxPath = 1 << 12;
constexpr size_t kBufSize = 1 << 12;

struct Entry {
  union {
    struct {
      char path[kMaxPath];
      struct stat status;
    } head;
    char head_bytes[sizeof(head)];
  };
  
  Entry() {}
  
  Entry(const char* path) {
    stat(path, &head.status);
    strcpy(head.path, path);
  }
  
  bool isDirectory() {
    return S_ISDIR(head.status.st_mode);
  }
  
  off_t size() {
    return head.status.st_size;
  }
  
  const char* path() {
    return head.path;
  }
  
  mode_t mode() {
    return head.status.st_mode;
  }
};

constexpr size_t kHeadSize = sizeof(Entry::head);

void addToArchive(FILE* har, const char* path) {
  Entry tba(path);
  
  if (tba.isDirectory()) {
    DIR* dir = opendir(path);
    if (!dir) {
      cerr << "har: failed to read " << path << endl;
      return;
    }
    
    // Writing Head
    fwrite(tba.head_bytes, 1, kHeadSize, har);
    
    // Going inside the directory...
    struct dirent* ent;
    vector<string> sub;
    while ((ent = readdir(dir))) {
      // Checking for "." and ".."
      if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
        continue;
      sub.push_back(string(path) + "/" + ent->d_name);
    }
    closedir(dir);
    
    // Adding subpaths in the archive
    for (auto sub_path: sub)
      addToArchive(har, sub_path.c_str());
  } else {
    FILE* file = fopen(path, "rb");
    if (!file) {
      cerr << "har: failed to read " << path << endl;
      return;
    }
    
    // Writing Head
    fwrite(tba.head_bytes, 1, kHeadSize, har);
    
    // Writing body
    char buf[kBufSize];
    while (!feof(file)) {
      size_t read = fread(buf, 1, kBufSize, file);
      fwrite(buf, 1, read, har);
    }
    
    fclose(file);
  }
}

void extractArchive(FILE* har) {
  Entry e;
  while (fread(e.head_bytes, kHeadSize, 1, har)) { // Read header
    if (e.isDirectory()) {
      mkdir(e.path(), e.mode());
    } else {
      size_t body_left = e.size();
      FILE* file = fopen(e.path(), "wb");
      if (!file) {
        cerr << "har: failed to extract " << e.path() << endl;
        continue;
      }
      
      bool could = true;
      
      // Give permissions
      chmod(e.path(), e.mode());
      
      char buf[kBufSize];
      int cnt = 0;
      while (body_left > 0) {
        ++cnt;
        size_t buf_size = min(kBufSize, body_left);
        size_t read = fread(buf, buf_size, 1, har);
        if (!read) {
          cerr << "har: failed to extract " << e.path() << endl;
          could = false;
          break;
        }
        body_left -= buf_size;
        fwrite(buf, 1, buf_size, file);
      }
      fclose(file);
      if (!could)
        return;
    }
  }
}

void listArchive(FILE* har) {
  Entry e;
  while (fread(e.head_bytes, kHeadSize, 1, har)) {
    cout << e.path() << endl;
    if (!e.isDirectory())
      fseek(har, e.size(), SEEK_CUR);
  }
}

int main(int argc, const char* argv[]) {
  if (argc < 2 || strlen(argv[1]) != 2 || argv[1][0] != '-') {
    cerr << "har: must specify one of -c, -x, -l" << endl;
    return 0;
  }
  
  if (argc < 3) {
    cerr << "har: no path specified" << endl;
    return 0;
  }
  
  char sw = argv[1][1];
  FILE *har;
  har = fopen(argv[2], sw == 'c'? "wb": "rb");
  
  if (!har) {
    cerr << "har: failed to access " << argv[2] << endl;
    return 0;
  }
  
  switch (sw) {
    case 'c':
      for (int i = 3; i < argc; ++i)
        addToArchive(har, argv[i]);
      break;
    case 'l':
      listArchive(har);
      break;
    case 'x':
      extractArchive(har);
      break;
    default:
      cerr << "har: must specify one of -c, -x, -l" << endl;
  }
  
  fclose(har);
  return 0;
}
