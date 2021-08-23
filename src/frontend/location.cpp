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

#include "location.h"

std::ostream & operator << (std::ostream &os, const Location &location) {
    os << location.filename;

    if (location.start.row != -1) {
        os << ":";
        if (location.start.row == location.end.row || location.end.row == -1) os << location.start.row;
        else os << "[" << location.start.row << "-" << location.end.row << "]";
    }

    if (location.start.column != -1) {
        os << ":";
        if (location.start.column == location.end.column || location.end.column == -1) os << location.start.column;
        else os << "[" << location.start.column << "-" << location.end.column << "]";
    }

    return os;
}
