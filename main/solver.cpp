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

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <list>
#include <string>
#include <queue>
#include <stack>
#include <tuple>
#include <vector>
#include <cassert>
#include <random>
#include <google/dense_hash_map>
#include "common.hpp"
#define GV_JS
// #define DISABLE_GV
#include "gv.hpp"

#define DEBUG_MEMORY_CHECK

using namespace std;

static const int dx[] = {1, 0, -1, 0};
static const int dy[] = {0, 1, 0, -1};

union Point {
  uint16_t value;
  struct {
    char x;
    char y;
  };
  Point() = default;
  Point(char x, char y) : x(x), y(y) {}
  Point operator+(const Point &o) const { return Point(x + o.x, y + o.y); }
  Point operator-(const Point &o) const { return Point(x - o.x, y - o.y); }
  bool operator==(const Point &o) const { return value == o.value; }
  void operator+=(const Point &o) {
    x += o.x;
    y += o.y;
  }
  void operator-=(const Point &o) {
    x -= o.x;
    y -= o.y;
  }
  bool operator<(const Point &o) const { return y != o.y ? y < o.y : x < o.x; }
  uint16_t to1d() const {
    assert(y >= 0);
    assert(x >= 0);
    return y << 5 | x;
  }
  static Point from1d(uint16_t p) {
    assert(!(p >> 10));
    return Point(p & 0x1f, p >> 5);
  }
};

struct Transform {
  static array<array<array<array<uint16_t, 32>, 32>, 8>, 8> trans_table_;
  static bool built;

 public:
  static Point trans(int from, int to, Point a) {
    assert(built);
    assert(from >= 0 && from < 8);
    assert(to >= 0 && to < 8);
    return Point::from1d(trans_table_[from][to][a.y][a.x]);
  }
  static Point trans(int from, int to, int x, int y) {
    assert(built);
    assert(from >= 0 && from < 8);
    assert(to >= 0 && to < 8);
    return Point::from1d(trans_table_[from][to][y][x]);
  }

  static void build() {
    if (built) return;
    built = true;

    array<array<uint16_t, 32>, 32> origin;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        origin[y][x] = y << 5 | x;
      }
    }

    auto flip = [](array<array<uint16_t, 32>, 32> f, bool x = true) {
      if (x) {
        for (int i = 0; i < 32; ++i) {
          std::reverse(f[i].begin(), f[i].end());
        }
      } else {
        for (int i = 0; i < 16; ++i) {
          std::swap(f[i], f[31 - i]);
        }
      }
      return std::move(f);
    };

    auto rotate90 = [flip](array<array<uint16_t, 32>, 32> f, bool plus = true) {
      for (int i = 0; i < 32; ++i) {
        for (int j = i; j < 32; ++j) {
          std::swap(f[i][j], f[j][i]);
        }
      }
      return std::move(flip(std::move(f), plus));
    };

    auto save = [](array<array<uint16_t, 32>, 32> f, int from, int to) {
      Transform::trans_table_[from][to] = f;
    };

    for (int i = 0; i < 8; ++i) {
      auto g = origin;
      save(g, i, i);
      for (int k = 1; k < 8; ++k) {
        int j = (i + k) % 8;
        g = rotate90(g);
        if (j % 4 == 0) g = flip(g);
        save(g, j, i);
      }
    }
  }
};
array<array<array<array<uint16_t, 32>, 32>, 8>, 8> Transform::trans_table_;
bool Transform::built = false;

struct FieldHash {
  static bool built;
  static void build() {
    built = true;
    std::mt19937 mt;
    // mt.seed(std::random_device()());
    for (int i = 0; i < hash_table_.size(); ++i) {
      hash_table_[i] = mt();
    }
  }
  static uint32_t get(int x, int y, int s) {
    assert(built);
    assert(x >= 0 && x < 32);
    assert(y >= 0 && y < 32);
    assert(s >= 1 && s <= 16);
    return hash_table_[y << 9 | x << 4 /*| (s - 1) */];
  }

 private:
  static array<uint32_t, 32 * 32 * 16> hash_table_;
};
bool FieldHash::built = false;
array<uint32_t, 32 * 32 * 16> FieldHash::hash_table_;

struct Field : public array<array<uint8_t, 32>, 32> {
  Field() {
    for (int i = 0; i < 32; ++i) (*this)[i].fill(0);
  };

  void transform(int from, int to) {
    Field f;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        auto p = Transform::trans(from, to, x, y);
        f[p.y][p.x] = (*this)[y][x];
      }
    }
    *this = std::move(f);
  }
};

struct BitField : public bitset<32 * 32> {
  size_t hash() const {
    static std::hash<bitset<32 * 32>> h;
    return h(*this);
  }
  void transform(int from, int to) {
    BitField f;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        auto p = Transform::trans(from, to, x, y);
        f[p.to1d()] = (*this)[y << 5 | x];
      }
    }
    *this = std::move(f);
  }
};

struct UsedMask : public bitset<256> {};

struct {
  vector<array<vector<Point>, 8>> stone_pattern_vector;
  vector<array<Point, 8>> stone_pattern_origin;
  array<array<bitset<256 * 8>, 16>, 8> stone_pattern_mask;
  array<Field, 8> default_field;
  int default_empty_count = 1024;
  int total_complete_count = 0;
  Problem problem;
  size_t total_alloc_count = 0;
  size_t total_free_count = 0;
  int weight_diff_id = 5;

  bool fix_ = false;
  void init(Problem problem_) {
    if (fix_) return;
    fix_ = true;
    problem = problem_;
    Transform::build();
    FieldHash::build();

    for (int y = 0; y < 32; y++) {
      for (int x = 0; x < 32; x++) {
        default_field[0][y][x] = problem_.field[y][x];
        if (problem_.field[y][x]) default_empty_count--;
      }
    }

    for (int i = 1; i < 8; ++i) {
      default_field[i] = default_field[0];
      default_field[i].transform(0, i);
    }

    vector<array<array<bitset<8>, 8>, 8>> stone_pattern_list_;
    stone_pattern_list_.resize(problem_.stone_list.size());
    for (int i = 0; i < problem_.stone_list.size(); ++i) {
      const auto &stone = problem_.stone_list[i];
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
          if (stone[y][x]) {
            stone_pattern_list_[i][0][y][x] = 1;
            stone_pattern_list_[i][4][y][7 - x] = 1;
          }
        }
      }
    }

    for (int i = 0; i < problem_.stone_list.size(); ++i) {
      for (int j = 1; j < 8; j++) {
        if (j % 4 == 0) continue;
        for (int y = 0; y < 8; y++) {
          for (int x = 0; x < 8; x++) {
            stone_pattern_list_[i][j][x][7 - y] =
                stone_pattern_list_[i][j - 1][y][x];
          }
        }
      }
    }

    stone_pattern_vector.resize(problem_.stone_list.size());
    stone_pattern_origin.resize(problem_.stone_list.size());
    for (int i = 0; i < problem_.stone_list.size(); ++i) {
      set<set<Point>> stone_vector_set;
      for (int j = 0; j < 8; j++) {
        set<Point> sample_vector;
        sample_vector.emplace(0, 0);
        bool found = false;
        Point p(0, 0);
        for (int y = 0; y < 8; y++) {
          for (int x = 0; x < 8; x++) {
            if (stone_pattern_list_[i][j][y][x]) {
              Point q(x, y);
              if (!found) {
                p = q;
                found = true;
              } else {
                sample_vector.emplace(q - p);
              }
            }
          }
        }

        auto result = stone_vector_set.emplace(sample_vector);
        if (result.second) {
          stone_pattern_origin[i][j] = p;
          for (const auto &p : sample_vector) {
            assert(p.x > -8 && p.x < 8);
            assert(p.y >= 0 && p.y < 8);
            stone_pattern_vector[i][j].emplace_back(p.x, p.y);
            stone_pattern_mask[p.y][p.x + 8][8 * i + j] = true;
          }
        } else {
          for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 16; ++x) {
              stone_pattern_mask[y][x][8 * i + j] = true;
            }
          }
        }
      }
    }
  }
} g;

template <class T>
bool isInsideField(T x, T y) {
  return !(x >> 5 | y >> 5);
}

template <class T>
bool isInsideField(const T &p) {
  return isInsideField(p.x, p.y);
}

union Put {
  uint32_t value;
  struct {
    u_char id;
    char y;
    char x;
    char r;
  };
  Put(u_char id, char x, char y, char r) : id(id), y(y), x(x), r(r) {}
};

vector<Put> backCalcPutList(const Field &f, const BitField &bf, UsedMask u) {
  vector<Put> ret;
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      if (bf[y << 5 | x] && u[f[y][x]]) {
        u[f[y][x]] = 0;
        vector<Point> stone;
        for (int sy = 0; sy < 8; ++sy) {
          for (int sx = -8; sx < 8; ++sx) {
            if (isInsideField(x + sx, y + sy)) {
              if (bf[(y + sy) << 5 | (x + sx)] &&
                  f[y][x] == f[y + sy][x + sx]) {
                stone.emplace_back(sx, sy);
              }
            }
          }
        }
        sort(stone.begin(), stone.end());
        bool found = false;
        for (int r = 0; r < 8; ++r) {
          if (g.stone_pattern_vector[f[y][x]][r] == stone) {
            ret.emplace_back(f[y][x], x, y, r);
            found = true;
            break;
          }
        }
        assert(found);
      }
    }
  }
  return std::move(ret);
}

class GameState {
 private:
  UsedMask used_;
  BitField bit_field_;
  Field field_;
  uint32_t hash_value_;
  int32_t score_;
  int16_t empty_count_;
  uint8_t trans_state_;

  GameState() = delete;
  GameState(const GameState &) = delete;
  ~GameState() = delete;

  static list<GameState *> pool_;

 public:
  GameState *init() {
    assert(g.fix_);
    used_ = UsedMask();
    bit_field_ = BitField();
    field_ = Field();
    empty_count_ = g.default_empty_count;
    trans_state_ = 0;
    hash_value_ = 0;
    score_ = 100000;
    return this;
  }

  static GameState *alloc() {
    if (pool_.empty()) {
      static const int N = 32768;
      GameState *p =
          static_cast<GameState *>(std::malloc(sizeof(GameState) * N));
      for (int i = 0; i < N; ++i) pool_.push_back(&p[i]);
    }
    GameState *p = pool_.back();
    pool_.pop_back();
#ifdef DEBUG_MEMORY_CHECK
    g.total_alloc_count++;
#endif
    return p;
  }

  static void free(GameState *p) {
    pool_.push_back(p);
#ifdef DEBUG_MEMORY_CHECK
    g.total_free_count++;
#endif
  }

  GameState *clone() const {
    GameState *q = alloc();
    q->used_ = used_;
    q->bit_field_ = bit_field_;
    q->field_ = field_;
    q->empty_count_ = empty_count_;
    q->trans_state_ = trans_state_;
    q->hash_value_ = hash_value_;
    q->score_ = score_;
    return q;
  }

  SolverAnswer toAnswer() const {
    GameState *s = this->clone();
    SolverAnswerBuilder builder(g.problem);
    s->transDefault();
    const auto log =
        backCalcPutList(s->getField(), s->getBitField(), s->getUsedMask());
    for (const auto &put : log) {
      const int x = put.x - g.stone_pattern_origin[put.id][put.r].x;
      const int y = put.y - g.stone_pattern_origin[put.id][put.r].y;
      builder.put(put.id, x, y, put.r / 4, put.r % 4);
    }
    GameState::free(s);
    return builder.build();
  }

  vector<Put> getAvailableList(int x, int y) const {
    vector<Put> ret;
    bitset<256 * 8> bits;
    for (int u = 0; u < 8; ++u) {
      for (int v = -7; v < 8; ++v) {
        const int ny = y + u;
        const int nx = x + v;
        if (isInsideField(nx, ny) && isEmpty(nx, ny))
          ;
        else {
          bits |= g.stone_pattern_mask[u][v + 8];
        }
      }
    }

    for (int i = 0; i < 256 * 8; ++i) {
      if (used_[i / 8] || bits[i]) {
        continue;
      } else {
        Put p = Put(i / 8, x, y, i % 8);
        if (available(p)) {
          ret.push_back(p);
        }
      }
    }
    return std::move(ret);
  }

  bool available(const Put &put, bool reverse = false) const {
    if (g.stone_pattern_vector.size() <= put.id) return false;
    if (isUsed(put.id)) return false;
    if (g.stone_pattern_vector[put.id][put.r].empty()) return false;
    bool ok = isFirstStone();
    for (auto pos : g.stone_pattern_vector[put.id][put.r]) {
      if (isInsideField(put.x + pos.x, put.y + pos.y) &&
          isEmpty(put.x + pos.x, put.y + pos.y))
        ;
      else
        return false;
      if (!ok) {
        for (int r = 0; r < 4; ++r) {
          const int x = put.x + dx[r];
          const int y = put.y + dy[r];
          if (isInsideField(x, y) && isStone(x, y)) {
            if (!reverse && field_[y][x] < put.id) {
              ok = true;
            }
            if (reverse && field_[y][x] > put.id) {
              ok = true;
            }
          }
        }
      }
    }
    return ok;
  }

  void put(const Put &put) {
    const auto &stone = g.stone_pattern_vector[put.id][put.r];
    const auto size = stone.size();
    assert(size);
    for (const auto &pos : stone) {
      const auto p = Point(put.x + pos.x, put.y + pos.y);
      assert(field_[p.y][p.x] == 0);
      assert(!bit_field_[p.to1d()]);
      field_[p.y][p.x] = put.id;
      bit_field_[p.to1d()] = 1;
      const auto q = Transform::trans(trans_state_, 0, p);
      hash_value_ ^= FieldHash::get(q.x, q.y, size);
    }
    assert(!used_[put.id]);
    used_[put.id] = 1;
    empty_count_ -= size;
  }

  void undo(const Put &put) {
    const auto &stone = g.stone_pattern_vector[put.id][put.r];
    const auto size = stone.size();
    assert(size);
    for (const auto &pos : stone) {
      const auto p = Point(put.x + pos.x, put.y + pos.y);
      assert(field_[p.y][p.x] == put.id);
      assert(bit_field_[p.to1d()]);
      field_[p.y][p.x] = 0;
      bit_field_[p.to1d()] = 0;
      const auto q = Transform::trans(trans_state_, 0, p);
      hash_value_ ^= FieldHash::get(q.x, q.y, size);
    }
    assert(used_[put.id]);
    used_[put.id] = 0;
    empty_count_ += size;
  }

  uint32_t preHash(const Put &put) const {
    uint32_t hash = hash_value_;
    const auto &stone = g.stone_pattern_vector[put.id][put.r];
    const auto size = stone.size();
    for (const auto &pos : stone) {
      const auto q =
          Transform::trans(trans_state_, 0, put.x + pos.x, put.y + pos.y);
      hash ^= FieldHash::get(q.x, q.y, size);
    }
    return hash;
  }

  int calcSimpleScore(const Put &put) const {
    int score = 0;
    const auto &stone = g.stone_pattern_vector[put.id][put.r];
    score += stone.size() * stone.size();
    if (isFirstStone()) {
      score -= (int)put.id * put.id;
    } else {
      int min_around_id = 255;
      for (const auto &pos : stone) {
        for (int r = 0; r < 4; ++r) {
          const int x = pos.x + put.x + dx[r];
          const int y = pos.y + put.y + dy[r];
          if (isInsideField(x, y)) {
            if (isStone(x, y) && field_[y][x] < put.id) {
              min_around_id = min(min_around_id, (int)field_[y][x]);
            }
          }
        }
      }
      score -= g.weight_diff_id * (put.id - min_around_id);
    }
    return score;
  }

  void gvField() const {
    int count = 0;
    for (int y = 0; y < 32; y++) {
      for (int x = 0; x < 32; x++) {
        if (g.default_field[trans_state_][y][x]) {
          if (bit_field_[y << 5 | x]) {
            gvRect(x, y, 1, 1, gvRGB(128, 0, 0));
          } else {
            count++;
            gvRect(x, y, 1, 1, gvRGB(0));
          }
        } else if (bit_field_[y << 5 | x]) {
          count++;
          gvRect(x, y, 1, 1, gvColor(field_[y][x]));
          gvText(x + 0.5, y + 0.5, 0.2, gvRGB(0, 0, 0), "%d", field_[y][x]);
        }
      }
    }
    gvText(10, -1, "%d/1024", count);
    gvRect(-20, 0, 19, 32, gvRGB(0xEEEEEE));
    int Y = 0;
    gvText(-10, Y++, "put_size = %d", used_.count());
    gvText(-10, Y++, "trans_state = %d", trans_state_);
    int cnt[17] = {0};
    for (int i = 0; i < g.stone_pattern_vector.size(); ++i) {
      if (!isUsed(i)) {
        cnt[g.stone_pattern_vector[i][0].size()]++;
      }
    }
    for (int i = 1; i <= 16; ++i) {
      gvText(-10, Y++, "%2d %3d", i, cnt[i]);
    }
  }

  void trans(int to) {
    assert(to >= 0 && to < 8);
    if (trans_state_ == to) return;
    field_.transform(trans_state_, to);
    bit_field_.transform(trans_state_, to);
    trans_state_ = to;
  }
  void trans() { trans((trans_state_ + 1) % 8); }
  void transDefault() { trans(0); }
  void transRotate(int r) {
    trans((trans_state_ >= 4) * 4 + (trans_state_ + r) % 4);
  }
  uint8_t getTransState() const { return trans_state_; }
  const Field &getField() const { return field_; }
  const BitField &getBitField() const { return bit_field_; }
  const UsedMask &getUsedMask() const { return used_; }
  bool isFirstStone() const { return !used_.any(); }
  bool isUsed(int i) const { return used_[i]; }
  int getEmptyCount() const { return empty_count_; }
  void addScore(int score) { score_ += score; };
  int getScore() const { return score_; }
  uint32_t getHash() const { return hash_value_; }
  bool isStone(int x, int y) const {
    assert(isInsideField(x, y));
    return bit_field_[y << 5 | x];
  }
  bool isBlock(int x, int y) const {
    assert(isInsideField(x, y));
    return g.default_field[trans_state_][y][x];
  }
  bool isEmpty(int x, int y) const {
    assert(isInsideField(x, y));
    return bit_field_[y << 5 | x] == 0 &&
           g.default_field[trans_state_][y][x] == 0;
  }
};
list<GameState *> GameState::pool_;

class SampleSolver {
 public:
  SampleSolver(const Problem &problem, SolverParameter &parameter)
      : problem_(problem), parameter_(parameter) {}

  SolverAnswer run() {
    hash_map.set_empty_key(0);
    gvInit();
    g.init(problem_);
    return solve();
  }

 private:
  vector<Point> getNextTargetPos(const GameState &state) {
    uint16_t f[32][32] = {{0}};
    int count = 0;
    union CountIdPair {
      uint32_t value;
      struct {
        uint16_t id;
        uint16_t count;
      };
      CountIdPair(uint16_t id, uint16_t count) : id(id), count(count) {}
      bool operator<(const CountIdPair &o) const { return value < o.value; }
    };
    vector<CountIdPair> count_id;
    queue<Point> Q;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        if (f[y][x] == 0 && state.isEmpty(x, y)) {
          count++;
          f[y][x] = count;
          count_id.emplace_back(count, 1);
          Q.emplace(x, y);
          while (!Q.empty()) {
            auto p = Q.front();
            Q.pop();
            for (int r = 0; r < 4; ++r) {
              const int nx = p.x + dx[r];
              const int ny = p.y + dy[r];
              if (isInsideField(nx, ny) && f[ny][nx] == 0 &&
                  state.isEmpty(nx, ny)) {
                f[ny][nx] = count;
                count_id[count - 1].count++;
                Q.emplace(nx, ny);
              }
            }
          }
        }
      }
    }

    if (count == 0) return {};
    auto best_area = min_element(count_id.begin(), count_id.end())->id;

    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        if (f[y][x] == best_area) {
          for (int r = 0; r < 4; ++r) {
            int nx = x + dx[r];
            int ny = y + dy[r];
            if (isInsideField(nx, ny)) {
              if (state.isStone(nx, ny)) {
                return {Point(x, y)};
              }
            }
          }
        }
      }
    }
    assert(false);
  }

  void beamSearch(vector<GameState *> &init, GameState **best, int beam_width) {
    vector<vector<GameState *>> state_av(1024);
    for (GameState *p : init) {
      assert(!p->isFirstStone());
      state_av[p->getEmptyCount()].push_back(p);
    }
    init.clear();

    auto add_next = [this, &state_av, &best](GameState *p, Put put, int score) {
      auto *s = p->clone();
      s->addScore(score);
      s->put(put);
      auto e = s->getEmptyCount();
      if ((*best)->getEmptyCount() > e ||
          ((*best)->getEmptyCount() == e &&
           (*best)->getUsedMask().count() > s->getUsedMask().count())) {
        GameState::free(*best);
        *best = s->clone();
      }
      if (e == 0) {
        g.total_complete_count++;
      }
      state_av[e].emplace_back(s);
      return true;
    };

    for (int k = 1023; k--;) {
      auto &v = state_av[k];
      if (v.empty()) continue;
      cerr << k << " " << v.size() << endl;

      const int n = min(beam_width, (int)v.size());
      if (v.size() > beam_width) {
        std::nth_element(v.begin(), v.begin() + beam_width, v.end(),
                         [](const GameState *a, const GameState *b) {
          return a->getScore() > b->getScore();
        });
        while (v.size() > beam_width) {
          GameState::free(v.back());
          v.pop_back();
        }
      }

      for (int i = 0; i < n; ++i) {
        auto &state = *v.back();
        auto pos_list = getNextTargetPos(state);
        for (auto &pos : pos_list) {
          auto available_list = state.getAvailableList(pos.x, pos.y);
          for (auto put : available_list) {
            // if (g.stone_pattern_vector[put.id][put.r].size() == 1) continue;
            const auto sc = state.calcSimpleScore(put);
            const auto hash = state.preHash(put);
            if (hash_map[hash] > state.getScore() + sc) continue;
            hash_map[hash] = state.getScore() + sc;
            add_next(&state, put, sc);
          }
        }
        GameState::free(&state);
        v.pop_back();
      }
      v.shrink_to_fit();
    }
  }

  void firstPut(int trans, vector<GameState *> &dst) {
    GameState *a = GameState::alloc()->init();
    a->trans(trans);
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        auto putList = a->getAvailableList(x, y);
        if (!putList.empty()) {
          for (auto put : putList) {
            GameState *b = a->clone();
            b->addScore(b->calcSimpleScore(put));
            b->put(put);
            dst.push_back(b);
          }
          GameState::free(a);
          return;
        }
      }
    }
    GameState::free(a);
  }

  SolverAnswer solve() {
    if (parameter_.extra_parameter.find("w") !=
        parameter_.extra_parameter.end()) {
      cerr << "setting weight" << parameter_.extra_parameter["w"] << endl;
      g.weight_diff_id = std::stoi(parameter_.extra_parameter["w"]);
      cerr << g.weight_diff_id << endl;
    }
    GameState *best_state = GameState::alloc()->init();
    size_t last_output_hash = best_state->getHash();
    auto best_t = 0;

    auto run_and_update = [this, &best_state, &last_output_hash, &best_t](
        int trans, int beam_width) {
      vector<GameState *> init;
      firstPut(trans, init);
      beamSearch(init, &best_state, beam_width);
      hash_map.clear_no_resize();
      if (last_output_hash != best_state->getHash()) {
        last_output_hash = best_state->getHash();
        best_t = trans;
        auto ans = best_state->toAnswer();
        auto ec = best_state->getEmptyCount();
        auto uc = (int)best_state->getUsedMask().count();
        char file_name[64];
        sprintf(file_name, "%04d_%03d.ans", ec, uc);
        SolverAnswerWriter(problem_, ans).write(file_name);
      }
    };

    for (int t = 0; t < 8; ++t) {
      run_and_update(t, 100);
    }

    /*
            for (int w = 1000; w <= 4000; w += 1000) {
                    run_and_update(best_t, w);
            }
     */

    best_state->gvField();
    gvNewTime();

    auto vis = GameState::alloc()->init();
    best_state->transDefault();
    for (auto put :
         backCalcPutList(best_state->getField(), best_state->getBitField(),
                         best_state->getUsedMask())) {
      vis->put(put);
      vis->gvField();
      gvNewTime();
    }

    auto ans = best_state->toAnswer();
    GameState::free(best_state);
    GameState::free(vis);

#ifdef DEBUG_MEMORY_CHECK
    assert(g.total_alloc_count == g.total_free_count);
#endif
    return ans;
  }

  const Problem &problem_;
  SolverParameter &parameter_;
  google::dense_hash_map<uint32_t, int> hash_map;
};

int main(int argc, char *argv[]) {
  auto parameter = SolverParameterParser().parse(argc, argv);
  auto problem = ProblemReader().read(parameter.problem_file);
  auto solver = SampleSolver(problem, parameter);
  auto answer = solver.run();
  if (parameter.answer_file.empty()) {
    SolverAnswerWriter(problem, answer).write(cout);
  } else {
    SolverAnswerWriter(problem, answer).write(parameter.answer_file);
  }
#ifdef DEBUG_MEMORY_CHECK
  cerr << "[last] alloc " << g.total_alloc_count << " free "
       << g.total_free_count << endl;
#endif
  return 0;
}
