/*
 * Copyright (C) 2017 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <sys/stat.h>

#include <iostream>
#include <fstream>
#include <string>

#include <zip.h>

#include <ignition/common/Console.hh>
#include <ignition/common/Filesystem.hh>

#include "ignition/fuel-tools/Zip.hh"

using namespace ignition;
using namespace fuel_tools;


/////////////////////////////////////////////////
bool CompressFile(zip *_archive, const std::string &_file,
    const std::string &_entry)
{
  std::string entry = ignition::common::basename(_file);
  if (ignition::common::isDirectory(_file))
  {
    if (zip_add_dir(_archive, _entry.c_str()) < 0)
    {
      ignerr << "Error adding directory to zip: " << _file << std::endl;
      return false;
    }

    ignition::common::DirIter endIt;
    for (ignition::common::DirIter dirIt(_file); dirIt != endIt; ++dirIt)
    {
      std::string file = *dirIt;
      std::string entryName = _entry + "/" + ignition::common::basename(file);

      if (!CompressFile(_archive, file, entryName))
      {
        ignerr << "Error compressing file: " << file << std::endl;
      }
    }
  }
  else if (ignition::common::isFile(_file))
  {
    std::ifstream in(_file.c_str(),
        std::ifstream::ate | std::ifstream::binary);
    auto end = in.tellg();

    zip_source* source = zip_source_file(_archive, _file.c_str(), 0, end);
    if (!source)
    {
      ignerr << "Error adding file to zip: " << _file << std::endl;
      return false;
    }

    if (zip_add(_archive, _entry.c_str(), source)
        < 0)
    {
      ignerr << "Error adding file to zip: " << _file << std::endl;
      zip_source_free(source);
      return false;
    }
  }
  return true;
}

/////////////////////////////////////////////////
bool Zip::Compress(const std::string &_src, const std::string &_dst)
{
  if (!ignition::common::exists(_src))
  {
    ignerr << "Archive does not exist: " << _src << std::endl;
    return false;
  }

  int err = 0;
  zip *archive = zip_open(_dst.c_str(), ZIP_CREATE, &err);
  if (!archive)
  {
    ignerr << "Error opening zip archive: '" << _dst << "'" << std::endl;
    return false;
  }

  std::string entry = ignition::common::basename(_src);
  if (!CompressFile(archive, _src, entry))
  {
    ignerr << "Error compressing file: " << _src << std::endl;
    return false;
  }

  zip_close(archive);
  return true;
}

/////////////////////////////////////////////////
bool Zip::Extract(const std::string &_src,
    const std::string &_dst)
{
  if (!ignition::common::exists(_src))
  {
    ignerr << "Source directory does not exist: " << _src << std::endl;
    return false;
  }

  int err;
  zip *archive = zip_open(_src.c_str(), 0, &err);
  if (!archive)
  {
    ignerr << "Error opening zip archive: '" << _src << "'" << std::endl;
    return false;
  }

  for (unsigned int i = 0; i < zip_get_num_entries(archive, 0); ++i)
  {
    struct zip_stat sb;
    if (zip_stat_index(archive, i, 0, &sb) != 0)
    {
      ignerr << "Error get stats on archive index: " << i << std::endl;
      continue;
    }

    // check if it's a directory
    std::string dst = _dst + "/" + sb.name;
    if (dst[dst.size() - 1] == '/')
    {
      ignition::common::createDirectory(dst);
    }
    else
    {
      zip_file * zf = zip_fopen_index(archive, i, 0);
      if (!zf)
      {
        ignerr << "Error opening: " << sb.name << std::endl;
        continue;
      }

      std::ofstream file(dst);
      int readSize = sb.size;
      char *buf = new char[readSize + 1];
      int len = zip_fread(zf, buf, readSize);
      // zip_fread seams to read more than requested resulting in garbage data
      // at the end so append '\0'
      buf[readSize] = '\0';
      if (len < 0)
        ignerr << "Error reading " << sb.name << std::endl;
      file << buf;
      delete[] buf;
      file.close();
      zip_fclose(zf);
    }
  }

  if (zip_close(archive) < 0)
  {
    ignerr << "Error closing zip archive" << std::endl;
    return false;
  }

  return true;
}
