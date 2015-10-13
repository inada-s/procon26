/*
The MIT License (MIT)

Copyright (c) 2015 Shingo INADA

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <array>
#include <cassert>

struct SolverParameter {
  std::string problem_file;
  std::string answer_file;
  std::map<std::string, std::string> extra_parameter;
};

struct Problem {
  std::vector<std::vector<int>> field;
  std::vector<std::vector<std::vector<int>>> stone_list;
};

struct SolverAnswer {
  struct Put {
    int stone_index, x, y, flip, rotate;
    Put(int stone_index, int x, int y, int flip, int rotate)
        : stone_index(stone_index), x(x), y(y), flip(flip), rotate(rotate) {}
  };
  std::vector<Put> put_list;
};

class SolverParameterParser {
 public:
  SolverParameter parse(int argc, char *argv[]) {
    SolverParameter param;
    bool extra = false;
    for (int i = 1; i < argc; ++i) {
      std::string arg(argv[i]);
      if (!extra) {
        if (arg == "-h" || arg == "--help") {
          printHelp();
          exit(0);
        }
        if (arg == "-i") {
          param.problem_file = argv[++i];
        } else if (arg == "-o") {
          param.answer_file = argv[++i];
        } else if (arg == "-ex") {
          extra = true;
        }
      } else {
        param.extra_parameter[argv[i]] = argv[i + 1];
        i++;
      }
    }
    if (param.problem_file.empty()) {
      printHelp();
      exit(1);
    }
    return std::move(param);
  }

 private:
  void printHelp() {
    std::cerr << "Usage: solver <options>" << std::endl;
    std::cerr << " -i problem_path" << std::endl;
    std::cerr << " -o problem_path (optional)" << std::endl;
    std::cerr << " -ex key1 value1 key2 value2 ... (optinal)" << std::endl;
  }
};

class ProblemReader {
 public:
  static Problem read(std::istream &is) {
    Problem problem;
    problem.field = std::vector<std::vector<int>>(32, std::vector<int>(32));
    std::string s;
    for (int y = 0; y < 32; ++y) {
      is >> s;
      for (int x = 0; x < 32; ++x) {
        problem.field[y][x] = s[x] == '1';
      }
    }
    int n;
    is >> n;
    problem.stone_list = std::vector<std::vector<std::vector<int>>>(
        n, std::vector<std::vector<int>>(8, std::vector<int>(8)));
    for (int i = 0; i < n; ++i) {
      for (int y = 0; y < 8; ++y) {
        is >> s;
        for (int x = 0; x < 8; ++x) {
          problem.stone_list[i][y][x] = s[x] == '1';
        }
      }
    }
    return std::move(problem);
  }

  Problem read(std::string problem_file) {
    std::ifstream ifs(problem_file);
    if (!ifs) {
      std::cerr << "cannnot open the problem_file. " << problem_file
                << std::endl;
      exit(1);
    }
    return read(ifs);
  }
};

class SolverAnswerBuilder {
 public:
  SolverAnswerBuilder(const Problem &problem)
      : stone_max_(static_cast<int>(problem.stone_list.size())) {}

  void put(int stone_index, int x, int y, int filp, int rotate) {
    assert(stone_index < stone_max_);
    assert(rotate >= 0 && rotate < 4);
    assert(x >= -7 && x < 32);
    assert(y >= -7 && y < 32);
    answer_.put_list.emplace_back(stone_index, x, y, filp, rotate);
  }

  SolverAnswer build() {
    std::sort(answer_.put_list.begin(), answer_.put_list.end(),
              [](const SolverAnswer::Put &a, const SolverAnswer::Put &b) {
      return a.stone_index < b.stone_index;
    });
    return answer_;
  }

 private:
  const int stone_max_;
  SolverAnswer answer_;
};

class SolverAnswerWriter {
 public:
  SolverAnswerWriter(const Problem &problem, const SolverAnswer &answer)
      : problem_(problem), answer_(answer) {}

  void write(std::ostream &os = std::cout) {
    auto it = answer_.put_list.begin();
    for (int i = 0; i < problem_.stone_list.size(); ++i) {
      if (it == answer_.put_list.end() || it->stone_index != i) {
        os << "\r\n";
      } else {
        os << it->x << " " << it->y << " " << (it->flip ? 'T' : 'H') << " "
           << it->rotate * 90 << "\r\n";
        ++it;
      }
    }
    os.flush();
  }

  void write(std::string answer_file) {
    std::ofstream ofs(answer_file);
    if (!ofs) {
      write(std::cout);
      std::cerr << "cannnot open the answer_file. " << answer_file << std::endl;
      return;
    }
    write(ofs);
  }

 private:
  const Problem &problem_;
  const SolverAnswer &answer_;
};
