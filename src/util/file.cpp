/*
 * Copyright 2019 SiFive, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You should have received a copy of LICENSE.Apache2 along with
 * this software. If not, you may obtain a copy at
 *
 *    https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _DEFAULT_SOURCE // MAP_ANON is not in POSIX yet

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <algorithm>
#include <fstream>

#include "file.h"
#include "diagnostic.h"

static uint8_t null[1] = { 0 };

FileContent::FileContent(const char *filename_)
 : fname(filename_)
{
    newlines.push_back(0);
}

FileContent::FileContent(FileContent &&o)
 : ss(o.ss), fname(std::move(o.fname)), newlines(std::move(o.newlines)) {
    o.ss.end = o.ss.start = &null[0];
}

FileContent &FileContent::operator = (FileContent &&o) {
    ss = o.ss;
    fname = std::move(o.fname);
    newlines = std::move(o.newlines);
    o.ss.end = o.ss.start = &null[0];
    return *this;
}

void FileContent::addNewline(const uint8_t *first_column)
{
    newlines.push_back(first_column - ss.start);
}

Coordinates FileContent::coordinates(const uint8_t *position) const
{
    size_t offset = position - ss.start;
    if (newlines.empty()) {
        return Coordinates(offset, 1);
    } else {
        auto it = std::upper_bound(newlines.begin(), newlines.end(), offset);
        --it; // always works, because newlines includes 0
        size_t row = 1 + (it - newlines.begin());
        size_t col = 1 + (offset - *it);
        return Coordinates(row, col);
    }
}

StringFile::StringFile(const char *filename_, std::string &&content_)
 : FileContent(filename_), content(std::move(content_))
{
    ss.start = reinterpret_cast<const uint8_t*>(content.c_str());
    ss.end = ss.start + content.size();
}

ExternalFile::ExternalFile(DiagnosticReporter &reporter, const char *filename_)
 : FileContent(filename_)
{
    Location l(filename());

#ifdef __EMSCRIPTEN__
    // mmap with specified address is broken...
    std::ifstream ifs(filename());
    if (!ifs) {
        reporter.reportError(l, std::string("open failed; ") + strerror(errno));
        ss.end = ss.start = &null[0];
        return;
    }

    ifs.seekg(0, std::ios::end);
    size_t length = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    uint8_t *base = new uint8_t[length+1];
    ifs.read(reinterpret_cast<char*>(base), length);

    base[length] = 0;
    ss.start = base;
    ss.end = base + length;
#else
    int fd = open(filename(), O_RDONLY);
    if (fd == -1) {
        reporter.reportError(l, std::string("open failed; ") + strerror(errno));
        ss.end = ss.start = &null[0];
        return;
    }

    struct stat s;
    if (fstat(fd, &s) == -1) {
        reporter.reportError(l, std::string("fstat failed; ") + strerror(errno));
        close(fd);
        ss.end = ss.start = &null[0];
        return;
    }

    // There is no need for a mapping if the file is empty.
    if (s.st_size == 0) {
        close(fd);
        ss.end = ss.start = &null[0];
        return;
    }

    // Map +1 over the length (for null ptr)
    void *map = mmap(nullptr, s.st_size+1, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        reporter.reportError(l, std::string("mmap failed; ") + strerror(errno));
        close(fd);
        ss.end = ss.start = &null[0];
        return;
    }

    // Now that the content is mapped, we don't need the file descriptor
    close(fd);
    ss.start = reinterpret_cast<const uint8_t*>(map);
    ss.end = ss.start + s.st_size;

    // We need to remap the tail of the file to ensure null termination
    long pagesize = sysconf(_SC_PAGESIZE);
    size_t tail_len = (s.st_size % pagesize) + 1;
    size_t tail_start = (s.st_size+1) - tail_len;

    std::string buf(reinterpret_cast<const char*>(map) + tail_start, tail_len-1);
    void *out = mmap(reinterpret_cast<uint8_t*>(map) + tail_start, pagesize, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANON, -1, 0);
    if (out == MAP_FAILED || ((char*)out - (char*)map) != tail_start) {
        reporter.reportError(l, std::string("mmap anon failed; ") + strerror(errno));
        munmap(map, s.st_size+1);
        ss.end = ss.start = &null[0];
        return;
    }

    // Fill in the last page
    memcpy(reinterpret_cast<uint8_t*>(map) + tail_start, buf.c_str(), tail_len);
#endif
}

ExternalFile::~ExternalFile() {
#ifdef __EMSCRIPTEN__
    if (ss.start != ss.end)
        delete [] ss.start;
#else
    if (ss.start != ss.end)
        munmap(const_cast<void*>(reinterpret_cast<const void*>(ss.start)), ss.size() + 1);
#endif
}

CPPFile::CPPFile(const char *filename)
 : FileContent(filename)
{
    newlines.clear();
}
