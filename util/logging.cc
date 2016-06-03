// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations under
// the License.

#include "util/logging.h"

#include <cassert>   // for assert()
#include <cstdlib>   // for abort()
#include <iostream>  // for std::cerr


namespace logle {
namespace util {

void Check(bool condition, const string& location, const string& err) {
  if (!condition) {
    std::cerr << location << ": " << err;
    std::abort();
  }
}

void Check(bool condition, const string& location) {
  Check(condition, location, "");
}

void Check(bool condition) { Check(condition, "", ""); }

}  // namespace util
}  // namespace logle