#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <parallel/algorithm>
#include <boost/lexical_cast.hpp>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "stddefines.h"

#define DEFAULT_DISP_NUM 10

enum {
  IN_KEY,
  IN_VALUE,
  NOT_IN_WORD
};

int main(int argc, char **argv)
{
  std::string value;
  std::string line;
  std::vector<uint64_t> data;
  struct timespec start, end;
  std::vector<std::pair<std::string, std::string>> kv_pairs;
  int fd;
  char *filename, *ndisp_str, *fdata;
  int ndisp;
  struct stat finfo;
  char *input_data;
  char *curr_word;
  char *curr_key;
  uint64_t r = 0;

  if (argv[1] == NULL) {
    printf("USAGE: %s <filename> <# results>\n", argv[0]);
    exit(1);
  }

  filename = argv[1];
  ndisp_str = argv[2];
  CHECK_ERROR((ndisp = ((ndisp_str == NULL) ?
    DEFAULT_DISP_NUM : atoi(ndisp_str))) <= 0);

  CHECK_ERROR((fd = open(filename, O_RDONLY)) < 0);
  CHECK_ERROR(fstat(fd, &finfo) < 0);

  printf("Mallocing the file: %lu\n", finfo.st_size);
  CHECK_ERROR((fdata = (char *) malloc(finfo.st_size + 1)) == NULL);
  while(r < finfo.st_size)
    r += pread (fd, fdata + r, finfo.st_size, r);
  CHECK_ERROR(r != (uint64_t) finfo.st_size);

  input_data = fdata;
  
    char curr_ltr;
  int state = IN_KEY;
  int nchar = 0;
  curr_word = input_data;
  curr_key = input_data;

  // Go through the file and update the count for each unique word
  for (uint64_t i = 0; i < finfo.st_size; i++) {

    // Each word is the same, regardless of case
    curr_ltr = input_data[i];
    switch (state) {
    case IN_KEY:
      if (nchar == 0)
        curr_word = &input_data[i];
      if (nchar >= 10) {
        // End of key
        state = NOT_IN_WORD;
        curr_key = curr_word;
        nchar = 0;
      }
      else
        nchar++;
      break;
    case IN_VALUE:
      if (curr_ltr == '\n') {
        // The values end in '\r\n'
        input_data[i] = 0;
        input_data[i - 1] = 0;

        // Add key value
        printf("<%s, %s>\n", curr_key, curr_word);
        
        // Modify the state
        state = IN_KEY;
      }
      break;
    default:
    case NOT_IN_WORD:
      if (curr_ltr >= '!' && curr_ltr <= '~') {
        // Start of a new word
        curr_word = &input_data[i];
        state = IN_VALUE;
      }
      break;
    }
  }  

  //while(std::getline(std::cin, value))
  //  data.push_back(boost::lexical_cast<uint64_t>(value));
  //int i = 0;

  //kv_pairs.push_back(std::make_pair("k1", "v3"));
  //kv_pairs.push_back(std::make_pair("a1", "v2"));
  //kv_pairs.push_back(std::make_pair("d1", "v1"));
  //kv_pairs.push_back(std::make_pair("a1", "v1"));
  //kv_pairs.push_back(std::make_pair("a1", "v3"));

  clock_gettime(CLOCK_REALTIME, &start);
  //__gnu_parallel::sort(kv_pairs.begin(), kv_pairs.end(), key_compare);
  std::sort(kv_pairs.begin(), kv_pairs.end());
  clock_gettime(CLOCK_REALTIME, &end);

  //std::cout << "Sorted " << data.size() << " values in "
  //  << "~" << (end.tv_sec - start.tv_sec) << " seconds."
  //  << std::endl;
  
  for (std::vector<std::pair<std::string, std::string>>::iterator it = kv_pairs.begin(); it != kv_pairs.end(); ++it)
    std::cout << "<" << std::get<0>(*it) << ", " << std::get<1>(*it) << ">\n";


  return 0;
}
