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

// Initializes the account access graph builds and prints the empty graph.
#include <iostream>
#include "account_access_graph.h"

int main(int argc, char **argv) {
  morphie::AccountAccessGraph graph;
  graph.Initialize();
  std::cout << "Initialized account access graph." << std::endl
            << graph.ToDot() << std::endl;
}
