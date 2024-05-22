#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <vector>
#include <cstdio>
#include <iostream>

static std::vector<unsigned> load_bmp(const char* filename, unsigned* pW, unsigned* pH)
{
  FILE* f = fopen(filename, "rb");

  if(f == nullptr)
  {
    (*pW) = 0;
    (*pH) = 0;
    std::cout << "can't open file" << std::endl;
    return {};
  }

  unsigned char info[138];
  auto readRes = fread(info, sizeof(unsigned char), 138, f); // read the 138-byte header
  if(readRes != 138)
  {
    std::cout << "can't read 138 byte BMP header" << std::endl;
    return {};
  }

  int width  = *(int*)&info[18];
  int height = *(int*)&info[22];

  int row_padded = width*4;
  auto data      = new unsigned char[row_padded];

  std::vector<unsigned> res(width*height);

  for(int i = 0; i < height; i++)
  {
    fread(data, sizeof(unsigned char), row_padded, f);
    for(int j = 0; j < width; j++)
      res[i*width+j] = (uint32_t(data[j*3]) << 24) | (uint32_t(data[j*3+3]) << 16) | (uint32_t(data[j*3+2]) << 8)  | (uint32_t(data[j*3+1]) << 0);
  }

  fclose(f);
  delete [] data;

  (*pW) = unsigned(width);
  (*pH) = unsigned(height);
  return res;
}
