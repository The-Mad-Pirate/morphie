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

// This file contains functions for initializing and running the different
// analyzers in logle. It also contains utility functions for the file I/O
// required to obtain the input for an analyzer or save the output generated by
// an analyzer.
#include "third_party/logle/frontend.h"

#include <fstream>
#include <memory>
#include <type_traits>
#include <utility>

#include "third_party/jsoncpp/json.h"
#include "third_party/logle/account_access_analyzer.h"
#include "third_party/logle/base/string.h"
#include "third_party/logle/curio_analyzer.h"
#include "third_party/logle/json_reader.h"
#include "third_party/logle/plaso_analyzer.h"
#include "third_party/logle/util/csv.h"
#include "third_party/logle/util/logging.h"
#include "third_party/logle/util/status.h"
#include "third_party/logle/util/string_utils.h"

namespace {

namespace logle = third_party_logle;
namespace util = third_party_logle::util;

// Error messages.
const char kInvalidAnalyzerErr[] =
    "Invalid analysis. The analysis must be one of 'curio', 'mail', or "
    "'plaso'.";
const char kOpenFileErr[] = "Error opening file: ";


// Returns a pair consisting of a status object and a CSV parser for 'filename'.
// The return value is:
//  - OK if 'filename' could be opened successfully. In this case, the second
//    element of the returned pair is a CSV parser and the input 'file_ptr'
//    points to the open file.
//  - An error code if the file could not be opened. No parser is returned and
//    'file_ptr' should not be used to read from the file.
//
// The file pointer is taken as a separate input to ensure it has a longer
// lifetime than the parser so that the file can be closed after parsing is
// complete.
std::pair<util::Status, std::unique_ptr<util::CSVParser>> GetCSVParser(
    const string& filename) {
  std::ifstream* csv_stream = new std::ifstream(filename);
  if (csv_stream == nullptr || !*csv_stream) {
    return {util::Status(logle::Code::EXTERNAL,
                         util::StrCat(kOpenFileErr, filename)), nullptr};
  }
  // The CSV parser takes ownership of the csv_stream and will close the file
  // once parsing is done.
  std::unique_ptr<util::CSVParser> parser(new util::CSVParser(csv_stream));
  return {util::Status::OK, std::move(parser)};
}

// Returns a JSON document extracted from 'filename'. Comments in the JSON
// document will be ignored.
std::unique_ptr<Json::Value> GetJsonDoc(const string& filename) {
  Json::Reader json_reader;
  std::unique_ptr<Json::Value> json_doc(new Json::Value);
  std::ifstream json_stream(filename);
  json_reader.parse(json_stream, *json_doc, false /*Do not parse comments*/);
  return json_doc;
}

// Writes the string 'contents' to 'filename'. Returns
//  - OK if 'filename' could be opened for writing, written to, and closed
//    successfully.
//  - an error code with explanation otherwise.
util::Status WriteToFile(const string& filename, const string& contents) {
  std::ofstream out_file;
  out_file.open(filename, std::ofstream::out);
  // An ofstream automatically closes a file when it goes out of scope, so the
  // early returns will not leave the file open. The file is nonetheless
  // explicitly closed only to be able to detect errors.
  if (!out_file) {
    return util::Status(logle::Code::EXTERNAL,
                       util::StrCat("Error opening file: ", filename));
  }
  out_file << contents;

  if (!out_file) {
    return util::Status(logle::Code::INTERNAL,
                       util::StrCat("Error writing to file: ", filename));
  }
  out_file.close();
  if (!out_file) {
    return util::Status(logle::Code::EXTERNAL,
                       util::StrCat("Error closing file: ", filename));
  }
  return util::Status::OK;
}

}  // namespace

namespace third_party_logle {
namespace frontend {

// Runs the Curio analyzer in curio_analyzer.h on the input. Returns an error
// code if the input is not in JSON format.
util::Status RunCurioAnalyzer(const AnalysisOptions& options,
                              string* dot_graph) {
  if (!options.has_json_file()) {
    return util::Status(logle::Code::INVALID_ARGUMENT,
                        "The Curio analyzer requires a JSON input file.");
  }
  std::unique_ptr<Json::Value> json_doc = GetJsonDoc(options.json_file());
  CurioAnalyzer curio_analyzer;
  util::Status status = curio_analyzer.Initialize(std::move(json_doc));
  if (!status.ok()) {
    return status;
  }
  status = curio_analyzer.BuildDependencyGraph();
  if (!status.ok()) {
    return status;
  }
  *dot_graph = curio_analyzer.DependencyGraphAsDot();
  return status;
}

// Runs the Plaso analyzer in plaso_analyzer.h on the input. The input can be in
// JSON or JSON stream format. Returns an error code if file I/O fails. If the
// analyzer is run successfully, a GraphViz DOT representation of the
// constructed graph is returned in 'dot_graph'.
util::Status RunPlasoAnalyzer(const AnalysisOptions& options,
                              string* dot_graph) {
  util::Status status;

  PlasoAnalyzer plaso_analyzer;
  switch (options.input_file_case()) {
    case AnalysisOptions::InputFileCase::kJsonFile:{
      std::ifstream file(options.json_file());
      CHECK(file.is_open(),
            util::StrCat(kOpenFileErr, options.json_file()));
      status = plaso_analyzer.Initialize(new logle::FullJson(&file));
      break;
    }
    case AnalysisOptions::InputFileCase::kJsonStreamFile:{
      std::ifstream file(options.json_stream_file());
      CHECK(file.is_open(),
            util::StrCat(kOpenFileErr, options.json_stream_file()));
      status = plaso_analyzer.Initialize(new logle::StreamJson(&file));
      break;
    }
    default:{
      FAIL("Unsupported input parameter. Plaso analyzer supports only "
           "json_file and json_stream_file.");
      break;
    }
  }
  if (!status.ok()) {
    return status;
  }
  plaso_analyzer.BuildPlasoGraph();
  *dot_graph = plaso_analyzer.PlasoGraphDot();
  return util::Status::OK;
}

// Runs the analyzer in account_access_analyzer.h on the input. Returns
//  - INVALID_ARGUMENT if the input is not in CSV format or if
//    file I/O causes an error or if graph initialization or construction fails.
//  - OK otherwise.
// If OK is returned, 'dot_graph' contains a GraphViz DOT graph.
util::Status RunMailAccessAnalyzer(const AnalysisOptions& options,
                                   string* dot_graph) {
  if (!options.has_csv_file()) {
    return util::Status(logle::Code::INVALID_ARGUMENT,
                        "The access analyzer requires a CSV input file.");
  }
  AccessAnalyzer access_analyzer;
  std::pair<util::Status, std::unique_ptr<util::CSVParser>> result =
      GetCSVParser(options.csv_file());

  util::Status status = result.first;
  if (!status.ok()) {
    return status;
  }
  status = access_analyzer.Initialize(std::move(result.second));
  if (!status.ok()) {
    return status;
  }
  status = access_analyzer.BuildAccessGraph();
  if (!status.ok()) {
    return status;
  }
  *dot_graph = access_analyzer.AccessGraphAsDot();
  return util::Status::OK;
}

// Invokes the specified analyzer. If the analysis generates a 'dot_graph', will
// write that graph to 'options.output_dot_file()' if that field is set.
util::Status Run(const AnalysisOptions& options) {
  util::Status status = util::Status::OK;
  string dot_graph;
  if (!options.has_analyzer()) {
    return util::Status(Code::INVALID_ARGUMENT, kInvalidAnalyzerErr);
  } else if (options.analyzer() == "curio") {
    status = RunCurioAnalyzer(options, &dot_graph);
  } else if (options.analyzer() == "mail") {
    status = RunMailAccessAnalyzer(options, &dot_graph);
  } else if (options.analyzer() == "plaso") {
    status = RunPlasoAnalyzer(options, &dot_graph);
  } else {
    return util::Status(Code::INVALID_ARGUMENT, kInvalidAnalyzerErr);
  }
  if (!status.ok() || dot_graph == "") {
    return status;
  }
  if (options.output_dot_file() != "") {
    status = WriteToFile(options.output_dot_file(), dot_graph);
  }
  return status;
}

}  // namespace frontend
}  // namespace third_party_logle
