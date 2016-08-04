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

// Uses the graph transformer to delete nodes in a graph and prints the result.
#include <iostream>

#include "ast.pb.h"
#include "dot_printer.h"
#include "graph_transformer.h"
#include "labeled_graph.h"
#include "type.h"
#include "value.h"

int main(int argc, char **argv) {
  logle::LabeledGraph graph;
  logle::AST ast = logle::ast::type::MakeInt("int label", false);
  logle::TaggedAST tast;
  tast.set_tag("num");
  *tast.mutable_ast() = logle::ast::value::MakeInt(0);
  graph.Initialize({{"num", ast}}, {}, {}, {}, ast);
  logle::NodeId node0 = graph.FindOrAddNode(tast);
  *tast.mutable_ast() = logle::ast::value::MakeInt(1);
  graph.FindOrAddNode(tast);
  logle::DotPrinter printer;
  std::cout << "Input graph." << std::endl
            << printer.DotGraph(graph) << std::endl
            << "Output graph." << std::endl
            << printer.DotGraph(*logle::graph::DeleteNodes(graph, {node0}))
            << std::endl;
}