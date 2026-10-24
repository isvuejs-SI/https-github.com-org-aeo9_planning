#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace aeo9 {

class CsvRunLogger {
 public:
  explicit CsvRunLogger(std::string path) : path_(std::move(path)) {}

  void log(int32_t time_ms, const std::string& state, int32_t energy_mwh,
           int32_t altitude_cm) {
    rows_.push_back(Row{time_ms, state, energy_mwh, altitude_cm});
  }

  void flush() {
    std::ofstream out(path_);
    out << "time_ms,state,energy_mwh,altitude_cm\n";
    for (const auto& row : rows_) {
      out << row.time_ms << ',' << row.state << ',' << row.energy_mwh << ','
          << row.altitude_cm << '\n';
    }
  }

  std::size_t row_count() const { return rows_.size(); }

 private:
  struct Row {
    int32_t time_ms;
    std::string state;
    int32_t energy_mwh;
    int32_t altitude_cm;
  };
  std::string path_;
  std::vector<Row> rows_;
};

}  // namespace aeo9
