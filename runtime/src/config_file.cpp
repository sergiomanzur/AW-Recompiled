#include "aw/config_file.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace aw {

namespace {

std::string trim(const std::string& str) {
  const auto first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

}  // namespace

bool ConfigFile::load(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) return false;

  data_.clear();
  std::string current_section = "General";
  std::string line;

  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;

    if (line.front() == '[' && line.back() == ']') {
      current_section = trim(line.substr(1, line.size() - 2));
    } else {
      const auto eq_pos = line.find('=');
      if (eq_pos != std::string::npos) {
        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));
        data_[current_section][key] = value;
      }
    }
  }

  return true;
}

bool ConfigFile::save(const std::string& filepath) const {
  std::ofstream file(filepath);
  if (!file.is_open()) return false;

  for (const auto& [section, keys] : data_) {
    file << "[" << section << "]\n";
    for (const auto& [key, value] : keys) {
      file << key << " = " << value << "\n";
    }
    file << "\n";
  }

  return true;
}

std::string ConfigFile::get_string(const std::string& section, const std::string& key, const std::string& default_val) const {
  auto sec_it = data_.find(section);
  if (sec_it != data_.end()) {
    auto key_it = sec_it->second.find(key);
    if (key_it != sec_it->second.end()) {
      return key_it->second;
    }
  }
  return default_val;
}

int ConfigFile::get_int(const std::string& section, const std::string& key, int default_val) const {
  const std::string val = get_string(section, key, "");
  if (val.empty()) return default_val;
  try {
    return std::stoi(val);
  } catch (...) {
    return default_val;
  }
}

void ConfigFile::set_string(const std::string& section, const std::string& key, const std::string& value) {
  data_[section][key] = value;
}

void ConfigFile::set_int(const std::string& section, const std::string& key, int value) {
  data_[section][key] = std::to_string(value);
}

}  // namespace aw
